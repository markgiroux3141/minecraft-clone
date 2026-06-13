#include "GameApp.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <random>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vox/core/Assets.h"
#include "vox/core/Log.h"
#include "vox/platform/Input.h"
#include "vox/renderer/Frustum.h"
#include "vox/renderer/Renderer.h"
#include "vox/renderer/Texture.h"

#include "Crafting.h"
#include "Item.h"
#include "ui/Hud.h"
#include "ui/InventoryScreen.h"
#include "ui/PauseMenu.h"
#include "ui/TitleScreen.h"
#include "world/Block.h"

namespace {

constexpr int kWorldSeed = 1337; // default for saves whose manifest lacks one
constexpr float kReachDistance = 5.0f;
constexpr double kEditRepeatDelay = 0.25; // held-button repeat, seconds

// M25: above the tallest terrain (~y99) in the 128-tall world, so a fresh
// world drops the player onto the surface instead of inside a hill. No fall
// damage system, so the short drop is harmless.
constexpr glm::vec3 kSpawnPos{8.5f, 104.0f, 8.5f};
constexpr float kSpawnYaw = 45.0f;
constexpr float kSpawnPitch = -15.0f;

// Day/night cycle: Minecraft's pacing (20 real minutes per day at 20 TPS).
// New worlds start shortly after sunrise; holding T fast-forwards.
constexpr double kDayTicks = 24000.0;
constexpr int64_t kNewWorldTime = 1000;
constexpr double kTimeFastForward = 200.0; // extra ticks per tick while T held

struct DayNight {
    glm::vec3 sunDir;
    glm::vec3 lightDir; // dominant body for diffuse: sun by day, moon by night
    float sunLight;     // sky-light scale, moonlight floor at night
    glm::vec3 skyTint;  // skylight color: white by day, cool blue moonlight
    glm::vec3 zenith;
    glm::vec3 horizon;
    glm::vec3 sunColor;
    glm::vec3 moonColor; // textured-moon tint; black while the sun is up
};

// t in [0,1): 0 sunrise, 0.25 noon, 0.5 sunset, 0.75 midnight.
DayNight ComputeDayNight(float t) {
    const float angle = t * glm::two_pi<float>();
    const float elevation = std::sin(angle); // -1..1
    DayNight dn;
    // East-west arc with a slight tilt so noon shadows aren't dead vertical.
    dn.sunDir = glm::normalize(glm::vec3(std::cos(angle), elevation, 0.25f));
    const float day = glm::smoothstep(-0.08f, 0.25f, elevation);
    // Moonlight floor 0.28: vanilla night terrain is clearly visible, just
    // dim and blue — 0.12 read nearly black.
    dn.sunLight = 0.28f + 0.72f * day;
    dn.lightDir = elevation >= 0.0f ? dn.sunDir : -dn.sunDir;
    dn.skyTint = glm::mix(glm::vec3(0.55f, 0.66f, 0.95f), glm::vec3(1.0f), day);
    dn.zenith = glm::mix(glm::vec3(0.015f, 0.025f, 0.07f), glm::vec3(0.22f, 0.51f, 0.92f), day);
    dn.horizon = glm::mix(glm::vec3(0.04f, 0.06f, 0.12f), glm::vec3(0.63f, 0.78f, 0.94f), day);
    // Warm band when the sun crosses the horizon, fading with elevation.
    const float sunset = std::max(0.0f, 1.0f - std::abs(elevation) * 5.0f);
    dn.horizon = glm::mix(dn.horizon, glm::vec3(0.93f, 0.49f, 0.26f), sunset * 0.65f);
    dn.sunColor = glm::mix(glm::vec3(1.0f, 0.45f, 0.20f), glm::vec3(1.0f, 0.95f, 0.85f), day);
    if (elevation < -0.15f) {
        dn.sunColor = glm::vec3(0.0f); // fully set — no disc
    }
    // The moon sits opposite the sun; fade it out as the sun climbs so it
    // never tracks visibly below the horizon gradient during the day.
    dn.moonColor = glm::vec3(0.9f) * (1.0f - glm::smoothstep(-0.05f, 0.15f, elevation));
    return dn;
}

// Real Minecraft assets imported by scripts/import_mc_assets.py land in the
// gitignored assets/mc/ overlay; prefer them so the committed placeholders
// still work on a clean clone (the imported set is personal-use only).
std::string PreferMcAsset(const std::string& relative) {
    const std::string overlay = "mc/" + relative;
    if (std::filesystem::exists(vox::assets::Resolve(overlay))) {
        return overlay;
    }
    return relative;
}

vox::ApplicationConfig MakeConfig() {
    vox::ApplicationConfig config;
    config.window.title = "Voxcraft";
    config.window.width = 1600;
    config.window.height = 900;
    config.window.vsync = true;
    config.tickRate = 20.0;
    return config;
}

} // namespace

GameApp::GameApp() : vox::Application(MakeConfig()) {}

void GameApp::OnInit() {
    GAME_INFO("Voxcraft starting up");
    vox::Renderer::SetClearColor(0.45f, 0.70f, 1.00f); // placeholder sky

    vc::blocks::RegisterDefaults();
    vc::items::RegisterDefaults();
    vc::Recipes::RegisterDefaults();

    m_chunkShader = vox::Shader::FromFiles("shaders/chunk.vert", "shaders/chunk.frag");
    // Model blocks (torches): float-position vertex shader, but the SAME
    // fragment shader as the cubic chunks — it produces identical varyings.
    m_modelShader = vox::Shader::FromFiles("shaders/model_block.vert", "shaders/chunk.frag");
    m_blockTextures =
        vox::Texture2DArray::FromFileStrip(PreferMcAsset("textures/atlas.png"), 16);

    m_ui = std::make_unique<vox::UiRenderer>();
    // Real MC font: 16x16 grid of 8x8 cells from char 0, proportional widths
    // scanned from the glyphs. Placeholder (scripts/gen_font.py): monospace
    // ASCII 32..127 in a 16x6 grid.
    const std::string fontPath = PreferMcAsset("fonts/ascii.png");
    if (fontPath.starts_with("mc/")) {
        m_ui->SetFont(vox::Texture2D::FromFile(fontPath), 16, 16, '\0', true);
    } else {
        m_ui->SetFont(vox::Texture2D::FromFile(fontPath), 16, 6);
    }
    if (std::filesystem::exists(vox::assets::Resolve("mc/textures/gui/icons.png"))) {
        m_guiTextures.icons = vox::Texture2D::FromFile("mc/textures/gui/icons.png");
    }
    if (std::filesystem::exists(vox::assets::Resolve("mc/textures/gui/widgets.png"))) {
        m_guiTextures.widgets = vox::Texture2D::FromFile("mc/textures/gui/widgets.png");
    }
    if (std::filesystem::exists(
            vox::assets::Resolve("mc/textures/gui/container/inventory.png"))) {
        m_guiTextures.inventory =
            vox::Texture2D::FromFile("mc/textures/gui/container/inventory.png");
    }
    if (std::filesystem::exists(
            vox::assets::Resolve("mc/textures/gui/container/crafting_table.png"))) {
        m_guiTextures.craftingTable =
            vox::Texture2D::FromFile("mc/textures/gui/container/crafting_table.png");
    }
    if (std::filesystem::exists(
            vox::assets::Resolve("mc/textures/gui/container/furnace.png"))) {
        m_guiTextures.furnace =
            vox::Texture2D::FromFile("mc/textures/gui/container/furnace.png");
    }

    m_skyShader = vox::Shader::FromFiles("shaders/sky.vert", "shaders/sky.frag");
    if (std::filesystem::exists(vox::assets::Resolve("mc/textures/environment/sun.png"))) {
        m_celestialShader =
            vox::Shader::FromFiles("shaders/celestial.vert", "shaders/celestial.frag");
        m_sunTexture = vox::Texture2D::FromFile("mc/textures/environment/sun.png");
        m_moonTexture = vox::Texture2D::FromFile("mc/textures/environment/moon_phases.png");
    }
    constexpr float skyVerts[] = {-1, -1, 1, -1, 1, 1, -1, 1};
    constexpr uint32_t skyIndices[] = {0, 1, 2, 2, 3, 0};
    auto skyBuffer = std::make_shared<vox::VertexBuffer>(
        skyVerts, static_cast<uint32_t>(sizeof(skyVerts)));
    skyBuffer->SetLayout({{vox::ShaderDataType::Float2, "a_position"}});
    m_skyQuad = std::make_shared<vox::VertexArray>();
    m_skyQuad->AddVertexBuffer(std::move(skyBuffer));
    m_skyQuad->SetIndexBuffer(std::make_shared<vox::IndexBuffer>(
        skyIndices, static_cast<uint32_t>(std::size(skyIndices))));

    m_entityShader =
        vox::Shader::FromFiles("shaders/block_entity.vert", "shaders/block_entity.frag");
    {
        // Unit cube, 4 verts per face in BlockFace order; drawn with
        // culling off, so winding doesn't matter.
        constexpr int kCorners[6][4][3] = {
            {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}}, // +X
            {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}, // -X
            {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}}, // +Y
            {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}, // -Y
            {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}, // +Z
            {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}, // -Z
        };
        constexpr float kNormals[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                                          {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        constexpr float kUv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        std::vector<float> verts;
        std::vector<uint32_t> indices;
        for (uint32_t face = 0; face < 6; ++face) {
            for (int c = 0; c < 4; ++c) {
                verts.insert(verts.end(),
                             {static_cast<float>(kCorners[face][c][0]),
                              static_cast<float>(kCorners[face][c][1]),
                              static_cast<float>(kCorners[face][c][2]), kNormals[face][0],
                              kNormals[face][1], kNormals[face][2], kUv[c][0], kUv[c][1],
                              static_cast<float>(face)});
            }
            const uint32_t base = face * 4;
            indices.insert(indices.end(),
                           {base, base + 1, base + 2, base + 2, base + 3, base});
        }
        auto cubeBuffer = std::make_shared<vox::VertexBuffer>(
            verts.data(), static_cast<uint32_t>(verts.size() * sizeof(float)));
        cubeBuffer->SetLayout({{vox::ShaderDataType::Float3, "a_position"},
                               {vox::ShaderDataType::Float3, "a_normal"},
                               {vox::ShaderDataType::Float2, "a_uv"},
                               {vox::ShaderDataType::Float, "a_face"}});
        m_entityCube = std::make_shared<vox::VertexArray>();
        m_entityCube->AddVertexBuffer(std::move(cubeBuffer));
        m_entityCube->SetIndexBuffer(std::make_shared<vox::IndexBuffer>(
            indices.data(), static_cast<uint32_t>(indices.size())));
    }
    {
        // Flat sprite quad for non-block item drops (sticks, tools) —
        // same vertex layout/shader as the cube, centered on z, both
        // sides shown (the entity pass draws with culling off).
        constexpr float quadVerts[] = {
            0, 0, 0.5f, 0, 0, 1, 0, 0, 0, //
            1, 0, 0.5f, 0, 0, 1, 1, 0, 0, //
            1, 1, 0.5f, 0, 0, 1, 1, 1, 0, //
            0, 1, 0.5f, 0, 0, 1, 0, 1, 0,
        };
        constexpr uint32_t quadIndices[] = {0, 1, 2, 2, 3, 0};
        auto quadBuffer = std::make_shared<vox::VertexBuffer>(
            quadVerts, static_cast<uint32_t>(sizeof(quadVerts)));
        quadBuffer->SetLayout({{vox::ShaderDataType::Float3, "a_position"},
                               {vox::ShaderDataType::Float3, "a_normal"},
                               {vox::ShaderDataType::Float2, "a_uv"},
                               {vox::ShaderDataType::Float, "a_face"}});
        m_itemQuad = std::make_shared<vox::VertexArray>();
        m_itemQuad->AddVertexBuffer(std::move(quadBuffer));
        m_itemQuad->SetIndexBuffer(std::make_shared<vox::IndexBuffer>(
            quadIndices, static_cast<uint32_t>(std::size(quadIndices))));
    }
    m_particles = std::make_unique<vc::ParticleSystem>();
    m_viewModel = std::make_unique<vc::ViewModel>(m_entityCube, m_itemQuad);

    m_outlineShader = vox::Shader::FromFiles("shaders/outline.vert", "shaders/outline.frag");
    constexpr float corners[] = {
        0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, // bottom-then-top ring order below
        0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1,
    };
    constexpr uint32_t edges[] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6,
                                  6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};
    auto cubeVerts = std::make_shared<vox::VertexBuffer>(
        corners, static_cast<uint32_t>(sizeof(corners)));
    cubeVerts->SetLayout({{vox::ShaderDataType::Float3, "a_position"}});
    m_outlineCube = std::make_shared<vox::VertexArray>();
    m_outlineCube->AddVertexBuffer(std::move(cubeVerts));
    m_outlineCube->SetIndexBuffer(std::make_shared<vox::IndexBuffer>(
        edges, static_cast<uint32_t>(std::size(edges))));

    // Saves live next to assets/ so every way of launching shares the worlds.
    m_savesRoot = vox::assets::Root().parent_path() / "saves";
    RefreshWorldList();

    m_camera.SetPerspective(70.0f, 0.1f, 1000.0f);
    m_camera.SetViewportSize(GetWindow().Width(), GetWindow().Height());

    // M22 audio: open the device and load the sound sets. Init failure (no
    // device) leaves a silent no-op engine — the game runs on mute. Clips load
    // from the gitignored assets/mc/sounds/ overlay; a clean clone is silent.
    m_audio.Init();
    m_sounds.Load(m_audio);

    // Starts on the title screen: no world yet, cursor free for the menu.
}

void GameApp::RefreshWorldList() {
    m_worlds.clear();
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(m_savesRoot, ec)) {
        if (entry.is_directory() && std::filesystem::exists(entry.path() / "level.dat")) {
            m_worlds.push_back(entry.path().filename().string());
        }
    }
    std::sort(m_worlds.begin(), m_worlds.end());
}

void GameApp::EnterWorld(const std::string& name, int defaultSeed) {
    GAME_INFO("Entering world '{}'", name);
    m_world = std::make_unique<vc::World>(defaultSeed, m_savesRoot / name);

    if (const auto& saved = m_world->SaveStore().GetPlayerState()) {
        m_player.Teleport(saved->position);
        m_player.SetLook(saved->yaw, saved->pitch);
        m_player.SetMode(saved->fly ? Player::Mode::Fly : Player::Mode::Walk);
    } else {
        m_player.Teleport(kSpawnPos);
        m_player.SetLook(kSpawnYaw, kSpawnPitch);
        m_player.SetMode(Player::Mode::Walk);
    }
    m_worldTime =
        static_cast<double>(m_world->SaveStore().GetWorldTime().value_or(kNewWorldTime));

    m_inventory.Clear();
    m_carried = {};
    m_craftGrid.fill({});
    m_hotbarSlot = 0;
    if (const auto& saved = m_world->SaveStore().GetInventory()) {
        for (const auto& s : *saved) {
            if (s.slot >= 0 && static_cast<size_t>(s.slot) < vc::Inventory::kSize &&
                vc::ItemExists(s.id) && s.count > 0) {
                m_inventory.Slot(static_cast<size_t>(s.slot)) = {
                    s.id, std::min(s.count, vc::ItemMaxStack(s.id)), std::max(s.damage, 0)};
            }
        }
    } else {
        // New world or pre-M17 save: the legacy hotbar as a starter kit
        // (slot 9 stays empty — the old empty-hand convention).
        const vc::BlockId kit[] = {vc::blocks::Stone, vc::blocks::Dirt,  vc::blocks::Grass,
                                   vc::blocks::Glowstone, vc::blocks::Sand, vc::blocks::Log,
                                   vc::blocks::Leaves, vc::blocks::Water};
        for (size_t i = 0; i < std::size(kit); ++i) {
            m_inventory.Slot(i) = {kit[i], vc::kMaxStackSize};
        }
    }

    m_state = State::Playing;
    m_target.reset();
    m_footInit = false; // re-seed footstep tracking at the new spawn
    GetWindow().SetCursorCaptured(true);
    // Same guard as unpausing: buttons held through the menu click must be
    // re-pressed before they edit the world.
    m_breakWasDown = true;
    m_placeWasDown = true;
    m_breakCooldown = kEditRepeatDelay;
    m_placeCooldown = kEditRepeatDelay;
}

void GameApp::CreateNewWorld() {
    std::string name = "world";
    for (int i = 2; std::filesystem::exists(m_savesRoot / name); ++i) {
        name = std::format("world{}", i);
    }
    // Fresh seed for fresh worlds; the manifest persists it from then on.
    EnterWorld(name, static_cast<int>(std::random_device{}()));
}

void GameApp::ExitToTitle() {
    PersistPlayerState();
    // Stop any furnace crackle loops before the world (and its furnaces) vanish.
    for (auto& [pos, voice] : m_furnaceLoops) {
        m_sounds.StopFurnaceLoop(voice);
    }
    m_furnaceLoops.clear();
    m_world.reset(); // ~World saves edited chunks and force-flushes the store
    m_target.reset();
    m_state = State::Title;
    GetWindow().SetCursorCaptured(false);
    RefreshWorldList();
}

void GameApp::PersistPlayerState() {
    if (!m_world) {
        return;
    }
    m_world->SaveStore().SetWorldTime(static_cast<int64_t>(m_worldTime));
    m_world->SaveStore().SetPlayerState({m_player.Position(), m_player.Yaw(), m_player.Pitch(),
                                         m_player.GetMode() == Player::Mode::Fly});
    // Merge any carried stack and craft-grid contents back first — the
    // only quit path that can have them in flight is the window X while a
    // container screen is open.
    m_inventory.Add(m_carried);
    m_carried = {};
    for (vc::ItemStack& cell : m_craftGrid) {
        m_inventory.Add(cell);
        cell = {};
    }
    std::vector<vc::WorldSave::InventorySlot> slots;
    for (size_t i = 0; i < vc::Inventory::kSize; ++i) {
        const vc::ItemStack& stack = m_inventory.Slot(i);
        if (!stack.Empty()) {
            slots.push_back({static_cast<int>(i), stack.id, stack.count, stack.damage});
        }
    }
    m_world->SaveStore().SetInventory(std::move(slots));
}

void GameApp::OnTick(double dt) {
    // Container screens don't pause the world (vanilla): block updates
    // run and the player keeps falling/floating, just without movement keys.
    if (m_world && (m_state == State::Playing || ContainerOpen())) {
        m_player.Tick(*m_world, dt, m_state == State::Playing);
        m_world->Tick(); // scheduled block updates (falling sand, water flow)
        m_particles->Tick(*m_world);
        m_viewModel->Tick(m_inventory.Slot(m_hotbarSlot));

        // Vacuum nearby drops: the player AABB grown by vanilla's pickup
        // reach (1.0, 0.5, 1.0); whatever the bag can't hold stays put.
        const glm::vec3 feet = m_player.Position();
        const glm::vec3 reach{Player::kHalfWidth + 1.0f, 0.5f, Player::kHalfWidth + 1.0f};
        bool pickedUp = false;
        m_world->PickupItems(feet - reach,
                             feet + glm::vec3{0.0f, Player::kHeight, 0.0f} + reach,
                             [&](uint16_t id, int count, int damage) {
                                 const vc::ItemStack leftover =
                                     m_inventory.Add({id, count, damage});
                                 const int taken = count - leftover.count;
                                 if (taken > 0) pickedUp = true;
                                 return taken;
                             });
        if (pickedUp) m_sounds.PlayPickup(); // one pop per tick max

        // M22: footsteps, landing thud, and a splash when entering water. The
        // supporting block's SoundType picks the footstep set; None (air/water)
        // makes PlayStep a no-op.
        constexpr float kStepStride = 1.7f; // blocks travelled between footsteps
        const auto soundUnder = [&](const glm::vec3& f) {
            const vc::BlockId b =
                m_world->GetBlock(static_cast<int>(std::floor(f.x)),
                                  static_cast<int>(std::floor(f.y - 0.2f)),
                                  static_cast<int>(std::floor(f.z)));
            return vc::BlockRegistry::Get().Def(b).soundType;
        };
        const bool grounded = m_player.Grounded();
        const bool inWater = m_player.InWater();
        if (!m_footInit) {
            m_lastFootPos = feet;
            m_wasGrounded = grounded;
            m_wasInWater = inWater;
            m_footInit = true;
        }
        if (inWater && !m_wasInWater) {
            m_sounds.PlaySplash(feet + glm::vec3{0.0f, 0.2f, 0.0f});
        }
        if (grounded && !m_wasGrounded && !inWater) {
            m_sounds.PlayLand(soundUnder(feet), feet);
            m_stepDistance = 0.0;
        }
        if (grounded && !inWater && m_state == State::Playing) {
            const float dx = feet.x - m_lastFootPos.x;
            const float dz = feet.z - m_lastFootPos.z;
            m_stepDistance += std::sqrt(dx * dx + dz * dz);
            if (m_stepDistance >= kStepStride) {
                m_stepDistance = 0.0;
                m_sounds.PlayStep(soundUnder(feet), feet);
            }
        }
        m_lastFootPos = feet;
        m_wasGrounded = grounded;
        m_wasInWater = inWater;

        m_worldTime += 1.0;
        if (m_state == State::Playing && vox::Input::IsKeyDown(vox::Key::T)) {
            m_worldTime += kTimeFastForward; // debug: watch the cycle quickly
        }
    }
    ++m_tickCount;
    ++m_totalTicks;
}

void GameApp::ThrowItem(const vc::ItemStack& stack) {
    if (!m_world || stack.Empty()) {
        return;
    }
    // Vanilla dropItem: from just below the eye, 0.3 b/tick (6 b/s) along
    // the look; 40-tick pickup delay so it isn't vacuumed straight back.
    const glm::vec3 dir = m_camera.Forward();
    const glm::vec3 origin = m_camera.Position() + glm::vec3{0.0f, -0.3f, 0.0f} + dir * 0.3f;
    m_world->SpawnItem(origin, dir * 6.0f, stack.id, stack.count, 40, stack.damage);
}

void GameApp::OpenContainer(State container) {
    if (!m_world || m_state != State::Playing) {
        return;
    }
    m_state = container;
    GetWindow().SetCursorCaptured(false);
    m_target.reset();
    m_digCell.reset();
    m_digProgress = 0.0f;
}

void GameApp::CloseContainer() {
    if (!ContainerOpen()) {
        return;
    }
    // Cursor stack and craft-grid contents go back to the bag (vanilla's
    // clearContainer); whatever doesn't fit is thrown at the feet.
    vc::ItemStack leftover = m_inventory.Add(m_carried);
    if (!leftover.Empty()) {
        ThrowItem(leftover);
    }
    m_carried = {};
    for (vc::ItemStack& cell : m_craftGrid) {
        leftover = m_inventory.Add(cell);
        if (!leftover.Empty()) {
            ThrowItem(leftover);
        }
        cell = {};
    }
    m_state = State::Playing;
    GetWindow().SetCursorCaptured(true);
    // Same guard as unpausing: buttons held through the closing click must
    // be re-pressed before they edit the world.
    m_breakWasDown = true;
    m_placeWasDown = true;
    m_breakCooldown = kEditRepeatDelay;
    m_placeCooldown = kEditRepeatDelay;
}

void GameApp::SetPaused(bool paused) {
    if (!m_world || (m_state == State::Paused) == paused) {
        return;
    }
    m_state = paused ? State::Paused : State::Playing;
    GetWindow().SetCursorCaptured(!paused);
    if (paused) {
        m_target.reset();
        m_digCell.reset();
        m_digProgress = 0.0f;
    } else {
        // Buttons held through the resume (e.g. the click that hit Resume)
        // must be re-pressed before they break or place anything.
        m_breakWasDown = true;
        m_placeWasDown = true;
        m_breakCooldown = kEditRepeatDelay;
        m_placeCooldown = kEditRepeatDelay;
    }
}

void GameApp::HandleInput(double frameDt, int scroll) {
    // F toggles walk/fly.
    const bool modeKey = vox::Input::IsKeyDown(vox::Key::F);
    if (modeKey && !m_modeKeyWasDown) {
        m_player.ToggleMode();
    }
    m_modeKeyWasDown = modeKey;

    // O toggles occlusion culling (debug/comparison; frustum-only when off).
    const bool occlusionKey = vox::Input::IsKeyDown(vox::Key::O);
    if (occlusionKey && !m_occlusionKeyWasDown) {
        m_occlusionCulling = !m_occlusionCulling;
        GAME_INFO("Occlusion culling {}", m_occlusionCulling ? "on" : "off");
    }
    m_occlusionKeyWasDown = occlusionKey;

    // 1..9 select the hotbar slot; the wheel cycles it (vanilla: scroll
    // up moves left, wrapping).
    for (size_t i = 0; i < vc::Inventory::kHotbarSize; ++i) {
        if (vox::Input::IsKeyDown(static_cast<vox::Key>(static_cast<int>(vox::Key::Num1) + i))) {
            m_hotbarSlot = i;
        }
    }
    if (scroll != 0) {
        constexpr int n = static_cast<int>(vc::Inventory::kHotbarSize);
        m_hotbarSlot = static_cast<size_t>(
            ((static_cast<int>(m_hotbarSlot) - scroll) % n + n) % n);
    }

    // Aim from the eye; break/place act on press, then repeat while held.
    m_target = m_world->RaycastBlocks(m_camera.Position(), m_camera.Forward(), kReachDistance);
    m_breakCooldown = std::max(0.0, m_breakCooldown - frameDt);
    m_placeCooldown = std::max(0.0, m_placeCooldown - frameDt);

    const bool breakDown = vox::Input::IsMouseButtonDown(vox::MouseButton::Left);
    if (m_player.GetMode() == Player::Mode::Fly) {
        // Creative-style (decided with the user): fly mode pops blocks
        // instantly and drops nothing — the palette is the source there.
        if (breakDown && m_target && (!m_breakWasDown || m_breakCooldown == 0.0)) {
            const vc::BlockId targetId =
                m_world->GetBlock(m_target->block.x, m_target->block.y, m_target->block.z);
            const vc::BlockDef& def = vc::BlockRegistry::Get().Def(targetId);
            if (!def.unbreakable) {
                m_world->SetBlock(m_target->block, vc::blocks::Air);
                m_particles->SpawnBlockDestroy(*m_world, m_target->block, targetId);
                m_sounds.PlayBreak(def.soundType, glm::vec3(m_target->block) + 0.5f);
                m_viewModel->TriggerSwing();
            }
            m_breakCooldown = kEditRepeatDelay;
        }
        m_digCell.reset();
        m_digProgress = 0.0f;
    } else if (breakDown && m_target) {
        // Survival dig (vanilla PlayerControllerMP.onPlayerDamageBlock):
        // damage accrues at digSpeed / hardness / 30 per tick — the
        // matching tool multiplies digSpeed by its efficiency (wood 2x,
        // stone 4x), head underwater and airborne each x1/5. Pickaxe-
        // gated blocks (stone family) use /100 instead of /30 and drop
        // nothing without one. The shared cooldown doubles as the 5-tick
        // post-break hit delay.
        const vc::BlockId targetId =
            m_world->GetBlock(m_target->block.x, m_target->block.y, m_target->block.z);
        const vc::BlockDef& def = vc::BlockRegistry::Get().Def(targetId);
        if (def.unbreakable || targetId == vc::blocks::Air) {
            m_digCell.reset();
            m_digProgress = 0.0f;
        } else {
            if (!m_digCell || *m_digCell != m_target->block) {
                m_digCell = m_target->block;
                m_digProgress = 0.0f;
                m_digSoundAccum = 1.0; // play a dig sound on the first hit
            }
            if (m_breakCooldown == 0.0) {
                // Vanilla digging feedback: continuous arm swing + one
                // hit chip from the dug face per tick.
                m_viewModel->TriggerSwing();
                m_chipAccum += frameDt;
                if (m_chipAccum >= 0.05) {
                    m_chipAccum = 0.0;
                    m_particles->SpawnBlockHit(*m_world, m_target->block, m_target->normal,
                                               targetId);
                }
                // Dig sound on the vanilla ~4-tick mining cadence.
                m_digSoundAccum += frameDt;
                if (m_digSoundAccum >= 0.2) {
                    m_digSoundAccum = 0.0;
                    m_sounds.PlayDig(def.soundType, glm::vec3(m_target->block) + 0.5f);
                }
                vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
                const vc::ItemDef* tool = vc::ItemRegistry::Get().Find(hand.id);
                // M21 tiers: the pickaxe must also reach the block's
                // harvest level (iron ore wants stone+).
                const bool canHarvest =
                    !def.needsPickaxe || (tool && tool->tool == vc::ToolClass::Pickaxe &&
                                          tool->tier >= def.harvestLevel);
                if (def.hardness <= 0.0f) {
                    m_digProgress = 1.0f; // plants: instant
                } else {
                    float digSpeed = 1.0f;
                    if (tool && tool->tool != vc::ToolClass::None &&
                        tool->tool == def.toolClass) {
                        digSpeed = tool->efficiency;
                    }
                    if (EyeInWater()) {
                        digSpeed *= 0.2f;
                    }
                    if (!m_player.Grounded()) {
                        digSpeed *= 0.2f;
                    }
                    // Per-second form of the per-tick formula (x20 TPS).
                    m_digProgress += static_cast<float>(frameDt) * digSpeed /
                                     (def.hardness * (canHarvest ? 1.5f : 5.0f));
                }
                if (m_digProgress >= 1.0f) {
                    m_world->SetBlock(m_target->block, vc::blocks::Air);
                    m_particles->SpawnBlockDestroy(*m_world, m_target->block, targetId);
                    m_sounds.PlayBreak(def.soundType, glm::vec3(m_target->block) + 0.5f);
                    if (canHarvest) {
                        m_world->SpawnBlockDrop(m_target->block, def.ResolveDrop(targetId), 1);
                    }
                    // Tools wear one use per broken block (vanilla: any
                    // block with hardness > 0) and break at zero.
                    if (tool && tool->maxDamage > 0 && def.hardness > 0.0f) {
                        if (++hand.damage >= tool->maxDamage) {
                            hand = {};
                        }
                    }
                    m_digCell.reset();
                    m_digProgress = 0.0f;
                    m_breakCooldown = kEditRepeatDelay; // vanilla blockHitDelay (5 ticks)
                }
            }
        }
    } else {
        m_digCell.reset();
        m_digProgress = 0.0f;
    }
    m_breakWasDown = breakDown;

    // Q tosses one item from the hand (press, then repeat while held).
    m_dropCooldown = std::max(0.0, m_dropCooldown - frameDt);
    const bool dropKey = vox::Input::IsKeyDown(vox::Key::Q);
    if (dropKey && (!m_dropKeyWasDown || m_dropCooldown == 0.0)) {
        vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
        if (!hand.Empty()) {
            ThrowItem({hand.id, 1, hand.damage});
            if (--hand.count <= 0) {
                hand = {};
            }
            m_dropCooldown = kEditRepeatDelay;
        }
    }
    m_dropKeyWasDown = dropKey;

    const bool placeDown = vox::Input::IsMouseButtonDown(vox::MouseButton::Right);
    if (placeDown && m_target && (!m_placeWasDown || m_placeCooldown == 0.0)) {
        const vc::BlockId targetId =
            m_world->GetBlock(m_target->block.x, m_target->block.y, m_target->block.z);
        if (targetId == vc::blocks::CraftingTable) {
            // Use beats place (vanilla, sans sneak): open the 3x3 grid.
            OpenContainer(State::Crafting);
            m_placeCooldown = kEditRepeatDelay;
        } else if (targetId == vc::blocks::Furnace || targetId == vc::blocks::LitFurnace) {
            m_openFurnace = m_target->block;
            OpenContainer(State::Furnace);
            m_placeCooldown = kEditRepeatDelay;
        } else {
            const glm::ivec3 cell = m_target->block + m_target->normal;
            vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
            if (!hand.Empty() && vc::IsBlockItem(hand.id) &&
                !m_world->IsSolid(cell.x, cell.y, cell.z) && !m_player.Intersects(cell)) {
                const auto blockId = static_cast<vc::BlockId>(hand.id);
                const vc::BlockDef& def = vc::BlockRegistry::Get().Def(blockId);
                // M24 orientation: torches mount on the clicked surface (top
                // face -> floor, side face -> wall, ceiling refused);
                // furnace/table fronts point back at the placer.
                uint8_t meta = 0;
                bool allowed = true;
                if (def.torch) {
                    const glm::ivec3 n = m_target->normal;
                    if (n.y > 0) {
                        allowed = m_world->IsSolid(cell.x, cell.y - 1, cell.z);
                        meta = vc::facing::TorchFloor;
                    } else if (n.y == 0) {
                        // The clicked face normal is the way the torch points.
                        const vc::BlockFace f = n.x > 0   ? vc::BlockFace::PosX
                                                : n.x < 0 ? vc::BlockFace::NegX
                                                : n.z > 0 ? vc::BlockFace::PosZ
                                                          : vc::BlockFace::NegZ;
                        allowed = m_world->IsSolid(m_target->block.x, m_target->block.y,
                                                   m_target->block.z);
                        meta = vc::facing::TorchWallMeta(f);
                    } else {
                        allowed = false; // bottom face: nothing to hang from
                    }
                } else if (def.horizontalFacing) {
                    meta = static_cast<uint8_t>(
                        vc::facing::Opposite(vc::facing::HorizontalFromLook(m_camera.Forward())));
                }
                if (allowed) {
                    m_world->SetBlock(cell, blockId, meta);
                    m_sounds.PlayPlace(def.soundType, glm::vec3(cell) + 0.5f);
                    m_viewModel->TriggerSwing();
                    if (--hand.count <= 0) {
                        hand = {};
                    }
                    m_placeCooldown = kEditRepeatDelay;
                }
            }
        }
    }
    m_placeWasDown = placeDown;
}

bool GameApp::EyeInWater() const {
    if (!m_world) {
        return false;
    }
    const glm::vec3 eye = m_camera.Position();
    return vc::BlockRegistry::Get()
        .Def(m_world->GetBlock(static_cast<int>(std::floor(eye.x)),
                               static_cast<int>(std::floor(eye.y)),
                               static_cast<int>(std::floor(eye.z))))
        .liquid;
}

void GameApp::DrawTargetOutline() {
    if (!m_target) {
        return;
    }
    m_outlineShader->Bind();
    m_outlineShader->SetMat4("u_viewProj", m_camera.ViewProjection());
    // Slightly inflated so the lines sit just outside the block's faces.
    glm::mat4 model =
        glm::translate(glm::mat4(1.0f), glm::vec3(m_target->block) - glm::vec3(0.002f));
    model = glm::scale(model, glm::vec3(1.004f));
    m_outlineShader->SetMat4("u_model", model);
    vox::Renderer::DrawLines(*m_outlineCube);
}

void GameApp::DrawUi() {
    const glm::vec2 screen{static_cast<float>(GetWindow().Width()),
                           static_cast<float>(GetWindow().Height())};
    const bool clickDown = vox::Input::IsMouseButtonDown(vox::MouseButton::Left);
    const bool clicked = clickDown && !m_clickWasDown;
    m_clickWasDown = clickDown;
    const bool rightDown = vox::Input::IsMouseButtonDown(vox::MouseButton::Right);
    const bool rightClicked = rightDown && !m_rightClickWasDown;
    m_rightClickWasDown = rightDown;

    const glm::vec2 mouse = vox::Input::MousePosition();

    m_ui->Begin(GetWindow().Width(), GetWindow().Height(), m_blockTextures.get());
    if (EyeInWater()) {
        // Underwater tint on top of the shader fog.
        m_ui->DrawRect({0.0f, 0.0f}, screen, {0.09f, 0.27f, 0.55f, 0.35f});
    }
    if (m_state == State::Title) {
        const auto action =
            vc::TitleScreen::Draw(*m_ui, screen, mouse, clicked, m_worlds, m_guiTextures);
        switch (action.type) {
        case vc::TitleScreen::Action::Type::Play:
            EnterWorld(m_worlds[action.worldIndex], kWorldSeed);
            break;
        case vc::TitleScreen::Action::Type::NewWorld:
            CreateNewWorld();
            break;
        case vc::TitleScreen::Action::Type::Quit:
            Close();
            break;
        case vc::TitleScreen::Action::Type::None:
            break;
        }
    } else {
        vc::Hud::Draw(*m_ui, screen, m_inventory.Hotbar(), m_hotbarSlot, m_guiTextures);
        if (ContainerOpen()) {
            // Vanilla's darkened-world backdrop behind the container GUI.
            m_ui->DrawRect({0.0f, 0.0f}, screen, {0.06f, 0.06f, 0.06f, 0.75f});
            vc::ItemStack thrown;
            if (m_state == State::Furnace) {
                vc::InventoryScreen::DrawFurnace(*m_ui, screen, mouse, clicked, rightClicked,
                                                 m_inventory,
                                                 m_world->FurnaceAt(m_openFurnace), m_carried,
                                                 thrown, m_guiTextures);
            } else {
                const bool table = m_state == State::Crafting;
                vc::InventoryScreen::Draw(
                    *m_ui, screen, mouse, clicked, rightClicked, m_inventory,
                    std::span<vc::ItemStack>{m_craftGrid.data(), table ? size_t{9} : size_t{4}},
                    table ? 3 : 2, m_carried, thrown, m_guiTextures);
            }
            if (!thrown.Empty()) {
                ThrowItem(thrown);
            }
        } else if (m_state == State::Paused) {
            const auto action = vc::PauseMenu::Draw(*m_ui, screen, mouse, clicked, m_guiTextures);
            if (action == vc::PauseMenu::Action::Resume) {
                SetPaused(false);
            } else if (action == vc::PauseMenu::Action::SaveQuit) {
                GAME_INFO("Save & Quit to title");
                ExitToTitle();
            }
        }
    }
    m_ui->End();
}

void GameApp::OnRender(double alpha, double frameDt) {
    const bool escapeDown = vox::Input::IsKeyDown(vox::Key::Escape);
    if (escapeDown && !m_escapeWasDown) {
        if (ContainerOpen()) {
            CloseContainer();
        } else {
            SetPaused(m_state == State::Playing); // no-op on the title screen
        }
    }
    m_escapeWasDown = escapeDown;

    // E opens the inventory / closes any container screen.
    const bool inventoryKey = vox::Input::IsKeyDown(vox::Key::E);
    if (inventoryKey && !m_inventoryKeyWasDown) {
        if (ContainerOpen()) {
            CloseContainer();
        } else {
            OpenContainer(State::Inventory);
        }
    }
    m_inventoryKeyWasDown = inventoryKey;

    // Consume wheel input every frame so clicks made over menus don't
    // burst-apply to the hotbar on resume.
    const int scroll = static_cast<int>(GetWindow().TakeScrollY());

    if (m_world) {
        const bool playing = m_state == State::Playing;
        m_player.OnRender(alpha, playing);
        if (playing) {
            HandleInput(frameDt, scroll);
        }
        m_world->Update(m_camera.Position());

        // M22 audio: the listener rides the camera; Update reaps finished
        // one-shots and advances fades. These run every frame (even paused) so
        // tails finish cleanly.
        const glm::vec3 eye = m_camera.Position();
        m_audio.SetListener(eye, m_camera.Forward(), glm::vec3{0.0f, 1.0f, 0.0f});
        m_audio.Update(frameDt);

        if (playing || ContainerOpen()) {
            // Keep one crackle loop per lit furnace: start new ones, stop those
            // that went out or unloaded (ForEachLitFurnace skips unloaded).
            std::vector<glm::ivec3> litThisFrame;
            m_world->ForEachLitFurnace([&](const glm::ivec3& pos) {
                litThisFrame.push_back(pos);
                auto it = m_furnaceLoops.find(pos);
                if (it == m_furnaceLoops.end() || !m_audio.IsVoiceActive(it->second)) {
                    m_furnaceLoops[pos] = m_sounds.StartFurnaceLoop(glm::vec3(pos) + 0.5f);
                }
            });
            for (auto it = m_furnaceLoops.begin(); it != m_furnaceLoops.end();) {
                if (std::find(litThisFrame.begin(), litThisFrame.end(), it->first) ==
                    litThisFrame.end()) {
                    m_sounds.StopFurnaceLoop(it->second);
                    it = m_furnaceLoops.erase(it);
                } else {
                    ++it;
                }
            }

            // Cave ambient when the eye sits in near-darkness; sparse music.
            const uint8_t light = m_world->PackedLightAt(glm::ivec3(glm::floor(eye)));
            const bool inDarkness = (light >> 4) < 4 && (light & 0x0F) < 4;
            m_sounds.UpdateAmbient(inDarkness, frameDt, eye);
            m_sounds.UpdateMusic(frameDt);
        }
    }

    vox::Renderer::Clear();

    if (m_world) {
        const double dayFraction = std::fmod(
            (m_worldTime + (m_state == State::Playing || ContainerOpen() ? alpha : 0.0)) /
                kDayTicks,
            1.0);
        const DayNight dayNight = ComputeDayNight(static_cast<float>(dayFraction));
        const glm::mat4 viewProj = m_camera.ViewProjection();
        const glm::vec3 eye = m_camera.Position();

        // Sky dome first: depth writes off, the terrain covers it after.
        m_skyShader->Bind();
        m_skyShader->SetMat4("u_invViewProj", glm::inverse(viewProj));
        m_skyShader->SetFloat3("u_eyePos", eye);
        m_skyShader->SetFloat3("u_zenithColor", dayNight.zenith);
        m_skyShader->SetFloat3("u_horizonColor", dayNight.horizon);
        m_skyShader->SetFloat3("u_sunDir", dayNight.sunDir);
        m_skyShader->SetFloat3("u_sunColor", dayNight.sunColor);
        m_skyShader->SetFloat("u_disc", m_sunTexture ? 0.0f : 1.0f);
        vox::Renderer::SetDepthWrite(false);
        vox::Renderer::DrawIndexed(*m_skyQuad);

        if (m_sunTexture) {
            // Textured sun + phased moon on eye-glued billboards, additive so
            // the sheets' black backgrounds vanish into the sky gradient.
            m_celestialShader->Bind();
            const glm::mat4 rotView = glm::mat4(glm::mat3(m_camera.View()));
            m_celestialShader->SetMat4("u_viewRotProj", m_camera.Projection() * rotView);
            m_celestialShader->SetInt("u_texture", 0);
            vox::Renderer::SetBlend(vox::BlendMode::Additive);

            const auto drawBody = [&](const glm::vec3& dir, float halfSize,
                                      const glm::vec3& tint, const vox::Texture2D& tex,
                                      glm::vec2 uvMin, glm::vec2 uvMax) {
                const glm::vec3 right =
                    glm::normalize(glm::cross(dir, glm::vec3(0.0f, 1.0f, 0.0f))) * halfSize;
                const glm::vec3 up = glm::normalize(glm::cross(right, dir)) * halfSize;
                m_celestialShader->SetFloat3("u_dir", dir);
                m_celestialShader->SetFloat3("u_right", right);
                m_celestialShader->SetFloat3("u_up", up);
                m_celestialShader->SetFloat3("u_tint", tint);
                m_celestialShader->SetFloat2("u_uvMin", uvMin);
                m_celestialShader->SetFloat2("u_uvMax", uvMax);
                tex.Bind(0);
                vox::Renderer::DrawIndexed(*m_skyQuad);
            };
            drawBody(dayNight.sunDir, 15.0f, dayNight.sunColor, *m_sunTexture, {0.0f, 0.0f},
                     {1.0f, 1.0f});
            // moon_phases.png: 4x2 grid of 32x32, full moon first; one phase
            // per day. The load flip puts image-row 0 at v 1.
            const auto phase =
                static_cast<int>(static_cast<int64_t>(m_worldTime / kDayTicks) % 8);
            const auto col = static_cast<float>(phase % 4);
            const auto row = static_cast<float>(phase / 4);
            drawBody(-dayNight.sunDir, 10.0f, dayNight.moonColor, *m_moonTexture,
                     {col * 0.25f, 1.0f - (row + 1.0f) * 0.5f},
                     {(col + 1.0f) * 0.25f, 1.0f - row * 0.5f});
            vox::Renderer::SetBlend(vox::BlendMode::None);
        }
        vox::Renderer::SetDepthWrite(true);

        m_chunkShader->Bind();
        m_chunkShader->SetMat4("u_viewProj", viewProj);
        m_chunkShader->SetInt("u_atlas", 0);
        m_chunkShader->SetFloat3("u_sunDir", dayNight.lightDir);
        m_chunkShader->SetFloat("u_sunLight", dayNight.sunLight);
        m_chunkShader->SetFloat3("u_skyTint", dayNight.skyTint);
        m_chunkShader->SetFloat3("u_eyePos", eye);
        if (EyeInWater()) {
            // Short, deep-blue fog; dims with the sky at night.
            m_chunkShader->SetFloat3("u_fogColor",
                                     glm::vec3(0.05f, 0.18f, 0.40f) * dayNight.sunLight);
            m_chunkShader->SetFloat2("u_fogRange", {2.0f, 28.0f});
        } else {
            // Fades into the horizon just inside the LOD shell's edge.
            m_chunkShader->SetFloat3("u_fogColor", dayNight.horizon);
            m_chunkShader->SetFloat2("u_fogRange", {320.0f, 500.0f});
        }
        m_blockTextures->Bind(0);

        const auto frustum = vox::Frustum::FromViewProjection(viewProj);
        m_world->CollectVisibleChunks(m_camera.Position(), frustum, m_occlusionCulling,
                                      m_drawItems, m_drawItemsTransparent, m_drawItemsModel);
        m_world->Meshes().Draw(m_drawItems);

        // Model blocks (torches): float-position geometry, alpha-tested in
        // the opaque pass (cull on, depth write on, blend off — the state
        // the chunk draw just left). Shares the chunk fragment shader, so it
        // takes the same lighting/fog uniforms.
        if (!m_drawItemsModel.empty()) {
            m_modelShader->Bind();
            m_modelShader->SetMat4("u_viewProj", viewProj);
            m_modelShader->SetInt("u_atlas", 0);
            m_modelShader->SetFloat3("u_sunDir", dayNight.lightDir);
            m_modelShader->SetFloat("u_sunLight", dayNight.sunLight);
            m_modelShader->SetFloat3("u_skyTint", dayNight.skyTint);
            m_modelShader->SetFloat3("u_eyePos", eye);
            if (EyeInWater()) {
                m_modelShader->SetFloat3("u_fogColor",
                                         glm::vec3(0.05f, 0.18f, 0.40f) * dayNight.sunLight);
                m_modelShader->SetFloat2("u_fogRange", {2.0f, 28.0f});
            } else {
                m_modelShader->SetFloat3("u_fogColor", dayNight.horizon);
                m_modelShader->SetFloat2("u_fogRange", {320.0f, 500.0f});
            }
            m_world->ModelMeshes().Draw(m_drawItemsModel);
            m_chunkShader->Bind();
        }

        // Entity cubes — falling blocks, item drops, and the dig crack
        // overlay share one shader and the unit-cube VAO. Opaque (alpha-
        // tested), so before the water pass (which expects the chunk
        // shader bound again afterwards).
        const auto& fallingBlocks = m_world->FallingBlocks();
        const auto& items = m_world->ItemEntities();
        const int crackStage =
            m_digCell ? std::min(static_cast<int>(m_digProgress * 10.0f) - 1, 9) : -1;
        if (!fallingBlocks.empty() || !items.empty() || crackStage >= 0) {
            m_entityShader->Bind();
            m_entityShader->SetMat4("u_viewProj", viewProj);
            m_entityShader->SetInt("u_atlas", 0);
            m_entityShader->SetFloat3("u_sunDir", dayNight.lightDir);
            m_entityShader->SetFloat("u_sunLight", dayNight.sunLight);
            m_entityShader->SetFloat3("u_skyTint", dayNight.skyTint);
            m_entityShader->SetFloat("u_unlit", 0.0f);
            vox::Renderer::SetCullFace(false); // a handful of cubes; skip winding care

            // Sample the cell AND the cell above and take the max: a cube
            // sitting in (or lingering over) an opaque cell reads light 0
            // and would flash black otherwise.
            const auto setCellLight = [&](const glm::ivec3& cell) {
                const uint8_t here = m_world->PackedLightAt(cell);
                const uint8_t above = m_world->PackedLightAt(cell + glm::ivec3{0, 1, 0});
                m_entityShader->SetFloat2(
                    "u_light",
                    {static_cast<float>(std::max(vc::ChunkLight::Sky(here),
                                                 vc::ChunkLight::Sky(above))) /
                         15.0f,
                     static_cast<float>(std::max(vc::ChunkLight::Block(here),
                                                 vc::ChunkLight::Block(above))) /
                         15.0f});
            };
            const auto setFaceLayers = [&](const std::array<uint16_t, 6>& tiles) {
                for (int face = 0; face < 6; ++face) {
                    m_entityShader->SetFloat(std::format("u_faceLayers[{}]", face),
                                             static_cast<float>(tiles[static_cast<size_t>(face)]));
                }
            };

            for (const auto& falling : fallingBlocks) {
                if (!m_world->FallingBlockVisible(falling)) {
                    continue; // mesh handover in progress (no double-draw/gap)
                }
                const float y = glm::mix(falling.prevY, falling.y, static_cast<float>(alpha));
                setFaceLayers(vc::BlockRegistry::Get().Def(falling.id).faceTiles);
                setCellLight({falling.x, static_cast<int>(std::floor(y + 0.5f)), falling.z});
                m_entityShader->SetFloat3("u_center", {static_cast<float>(falling.x) + 0.5f,
                                                       y + 0.5f,
                                                       static_cast<float>(falling.z) + 0.5f});
                m_entityShader->SetFloat("u_scale", 1.0f);
                m_entityShader->SetFloat("u_yaw", 0.0f);
                vox::Renderer::DrawIndexed(*m_entityCube);
            }

            for (const auto& item : items) {
                // Vanilla item presentation: hovers (sin, ~3 s period)
                // and spins (~6 s per turn). Block drops are quarter-
                // scale mini cubes; registry items and sprite-like
                // blocks (plants, torches) are flat alpha-tested quads.
                const glm::vec3 pos =
                    glm::mix(item.prevPos, item.pos, static_cast<float>(alpha));
                const float t = static_cast<float>(item.age) + static_cast<float>(alpha);
                const float bob = std::sin(t * 0.1f + item.phase) * 0.1f + 0.1f;
                const bool block = !vc::RenderAsSprite(item.id);
                if (block) {
                    setFaceLayers(vc::BlockRegistry::Get().Def(item.id).faceTiles);
                } else {
                    std::array<uint16_t, 6> spriteTiles;
                    spriteTiles.fill(vc::ItemIconTile(item.id));
                    setFaceLayers(spriteTiles);
                }
                const float scale = block ? 0.25f : 0.4f;
                setCellLight({static_cast<int>(std::floor(pos.x)),
                              static_cast<int>(std::floor(pos.y + 0.125f)),
                              static_cast<int>(std::floor(pos.z))});
                m_entityShader->SetFloat3("u_center",
                                          pos + glm::vec3{0.0f, scale * 0.5f + bob, 0.0f});
                m_entityShader->SetFloat("u_scale", scale);
                m_entityShader->SetFloat("u_yaw", t * 0.05f + item.phase);
                vox::Renderer::DrawIndexed(block ? *m_entityCube : *m_itemQuad);
            }

            if (crackStage >= 0) {
                // Crack overlay, vanilla's crumbling pass: an alpha-tested
                // cube inflated just past the block's faces, drawn UNLIT
                // with the 2*src*dst Crumble blend — the texture's dark
                // pixels darken the (already lit) block, its light pixels
                // highlight, and backfaces hide behind the opaque mesh.
                std::array<uint16_t, 6> crackTiles;
                crackTiles.fill(static_cast<uint16_t>(vc::blocks::kFirstCrackTile + crackStage));
                setFaceLayers(crackTiles);
                m_entityShader->SetFloat("u_unlit", 1.0f);
                m_entityShader->SetFloat3("u_center", glm::vec3(*m_digCell) + 0.5f);
                m_entityShader->SetFloat("u_scale", 1.004f);
                m_entityShader->SetFloat("u_yaw", 0.0f);
                vox::Renderer::SetBlend(vox::BlendMode::Crumble);
                vox::Renderer::SetDepthWrite(false);
                vox::Renderer::DrawIndexed(*m_entityCube);
                vox::Renderer::SetBlend(vox::BlendMode::None);
                vox::Renderer::SetDepthWrite(true);
            }

            vox::Renderer::SetCullFace(true);
            m_chunkShader->Bind();
        }

        if (!m_drawItemsTransparent.empty()) {
            // Water: blended, depth-tested but not depth-written, double-
            // sided so the surface shows from below too.
            vox::Renderer::SetBlend(vox::BlendMode::Alpha);
            vox::Renderer::SetDepthWrite(false);
            vox::Renderer::SetCullFace(false);
            m_world->Meshes().Draw(m_drawItemsTransparent);
            vox::Renderer::SetBlend(vox::BlendMode::None);
            vox::Renderer::SetDepthWrite(true);
            vox::Renderer::SetCullFace(true);
        }
        m_chunksDrawn = m_drawItems.size();

        // Break particles after the water (vanilla's pass order).
        m_particles->Render(m_camera, m_state == State::Playing || ContainerOpen() ? alpha : 0.0,
                            dayNight.sunLight, dayNight.skyTint);

        DrawTargetOutline();

        // First-person hand, last: it draws over a cleared depth buffer.
        const glm::ivec3 eyeCell{static_cast<int>(std::floor(eye.x)),
                                 static_cast<int>(std::floor(eye.y)),
                                 static_cast<int>(std::floor(eye.z))};
        const uint8_t eyeLight = m_world->PackedLightAt(eyeCell);
        m_viewModel->Render(m_camera, m_state == State::Playing || ContainerOpen() ? alpha : 0.0,
                            {static_cast<float>(vc::ChunkLight::Sky(eyeLight)) / 15.0f,
                             static_cast<float>(vc::ChunkLight::Block(eyeLight)) / 15.0f},
                            dayNight.sunLight, dayNight.skyTint);
    }

    DrawUi();

    ++m_frameCount;
    m_statsTimer += frameDt;
    if (m_statsTimer >= 1.0) {
        if (m_world) {
            m_chunksWithMesh = 0;
            m_trianglesLoaded = 0;
            m_world->ForEachRenderableChunk(
                [&](const glm::ivec3&, vox::MeshPool::MeshHandle, uint32_t indexCount) {
                    ++m_chunksWithMesh;
                    m_trianglesLoaded += indexCount / 3;
                });
            const auto pos = m_camera.Position();
            // In-world clock: t=0 is sunrise at 06:00.
            const int clockMinutes = static_cast<int>(
                std::fmod(m_worldTime / kDayTicks * 24.0 + 6.0, 24.0) * 60.0);
            GetWindow().SetTitle(std::format(
                "Voxcraft | {} fps | {} tps | ({:.0f}, {:.0f}, {:.0f}) | {:02}:{:02} | {} | "
                "hand: {} | chunks: "
                "{} loaded, {}/{} drawn, {} pending | {} jobs | {:.2f}M tris | pool {}/{} MB",
                m_frameCount, m_tickCount, pos.x, pos.y, pos.z, clockMinutes / 60,
                clockMinutes % 60,
                m_player.GetMode() == Player::Mode::Fly ? "fly" : "walk",
                vc::ItemName(m_inventory.Slot(m_hotbarSlot).id),
                m_world->LoadedChunkCount(), m_chunksDrawn, m_chunksWithMesh,
                m_world->PendingMeshCount(), m_world->JobsInFlight(),
                static_cast<double>(m_trianglesLoaded) / 1e6,
                m_world->Meshes().UsedVertices() * sizeof(vc::ChunkVertex) / (1024 * 1024),
                m_world->Meshes().CapacityVertices() * sizeof(vc::ChunkVertex) / (1024 * 1024)));
        } else {
            GetWindow().SetTitle(std::format("Voxcraft | {} fps | title screen", m_frameCount));
        }
        m_statsTimer -= 1.0;
        m_frameCount = 0;
        m_tickCount = 0;
    }
}

void GameApp::OnResize(uint32_t width, uint32_t height) {
    m_camera.SetViewportSize(width, height);
}

void GameApp::OnShutdown() {
    PersistPlayerState(); // window-close path; ExitToTitle covers the menu path
    m_audio.Shutdown();   // stop the audio thread before members tear down
    GAME_INFO("Voxcraft shutting down after {} ticks", m_totalTicks);
}
