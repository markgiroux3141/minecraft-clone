#include "GameApp.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <random>
#include <string_view>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vox/core/Assets.h"
#include "vox/core/Log.h"
#include "vox/platform/Input.h"
#include "vox/renderer/Frustum.h"
#include "vox/renderer/Renderer.h"
#include "vox/renderer/Texture.h"

#include "item/Crafting.h"
#include "item/Item.h"
#include "ui/Hud.h"
#include "ui/InventoryScreen.h"
#include "ui/PauseMenu.h"
#include "ui/TitleScreen.h"
#include "world/Block.h"

namespace {

constexpr int kWorldSeed = 1337; // default for saves whose manifest lacks one
constexpr float kReachDistance = 5.0f;
constexpr double kEditRepeatDelay = 0.25; // held-button repeat, seconds
// M37: vanilla getMaxItemUseDuration is 32 ticks (1.6 s) to eat a food, with a
// chewing crunch + crumb particles emitted every few ticks during the hold.
constexpr double kEatSeconds = 1.6;
constexpr double kEatChewInterval = 0.30;

// M28: which half a slab/stair takes when placed (vanilla BlockSlab/
// BlockStairs rule): top if you clicked a downward face, bottom if you
// clicked an upward face, else the half of the side face you clicked.
bool PlaceTopHalf(const glm::ivec3& normal, const glm::vec3& hitPoint) {
    if (normal.y > 0) {
        return false; // clicked a top face -> rests on it (bottom half)
    }
    if (normal.y < 0) {
        return true; // clicked a bottom face -> hangs under it (top half)
    }
    return hitPoint.y - std::floor(hitPoint.y) > 0.5f; // side face: upper half?
}

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

    // M29: 3D iso block icons for inventory/hotbar slots, baked from the atlas.
    m_blockIcons = std::make_unique<vc::BlockIcons>(m_blockTextures);
    m_guiTextures.blockIcons = m_blockIcons.get();

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
    if (std::filesystem::exists(vox::assets::Resolve("mc/textures/fire_layer_1.png"))) {
        m_fireOverlay = vox::Texture2D::FromFile("mc/textures/fire_layer_1.png");
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

    // M31 jointed box-model renderer + the debug Steve test model.
    m_entityModelShader =
        vox::Shader::FromFiles("shaders/entity_model.vert", "shaders/entity_model.frag");
    m_humanoid = std::make_unique<vc::HumanoidModel>();
    m_playerDoll = std::make_unique<vc::PlayerDoll>(); // M33 inventory player doll
    // Mob models, indexed by MobType. The zombie is the biped with the zombie
    // skin + arms-out pose; the rest are species models. Each silently skips
    // drawing without its gitignored skin overlay (like the debug Steve), so a
    // clean clone is fine. Adding a mob = one model + one line here.
    m_mobModels[static_cast<size_t>(vc::MobType::Pig)] = std::make_unique<vc::PigModel>();
    m_mobModels[static_cast<size_t>(vc::MobType::Zombie)] = std::make_unique<vc::HumanoidModel>(
        "mc/textures/entity/zombie/zombie.png", vc::HumanoidModel::Pose::Zombie);
    m_mobModels[static_cast<size_t>(vc::MobType::Cow)] = std::make_unique<vc::CowModel>();
    m_mobModels[static_cast<size_t>(vc::MobType::Sheep)] = std::make_unique<vc::SheepModel>();
    m_mobModels[static_cast<size_t>(vc::MobType::Chicken)] = std::make_unique<vc::ChickenModel>();
    m_mobModels[static_cast<size_t>(vc::MobType::Creeper)] = std::make_unique<vc::CreeperModel>();
    // M36 skeleton: the biped with the skeleton skin, thin (2px) limbs, and the
    // bow-aim pose (shown only while it's drawing — see the mob render pass).
    // The 1.12 skeleton skin is 64x32 (ModelSkeleton: super(.., 64, 32)), NOT
    // 64x64 like the zombie — pass texH=32 or every UV island samples the wrong
    // rows and the model renders as a featureless grey blob.
    m_mobModels[static_cast<size_t>(vc::MobType::Skeleton)] = std::make_unique<vc::HumanoidModel>(
        "mc/textures/entity/skeleton/skeleton.png", vc::HumanoidModel::Pose::BowAim, 0.0f, 64.0f,
        32.0f, true, /*thinArms=*/true);
    // M39 spider: the eight-legged climber (64x32 skin, like the cow).
    m_mobModels[static_cast<size_t>(vc::MobType::Spider)] = std::make_unique<vc::SpiderModel>();
    m_arrowModel = std::make_unique<vc::ArrowModel>(); // M36 flying-arrow renderer
    m_heldBow = std::make_unique<vc::HeldBowModel>();  // M36 bow in the skeleton's hand

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
        // New world or pre-M17 save: the legacy hotbar as a starter kit.
        // Slot 9 carries lava (M26) so it's one keypress away for testing —
        // water sits in slot 8 right beside it for mixing experiments.
        const vc::BlockId kit[] = {vc::blocks::Stone, vc::blocks::Dirt,    vc::blocks::Grass,
                                   vc::blocks::Glowstone, vc::blocks::Sand, vc::blocks::Log,
                                   vc::blocks::Leaves, vc::blocks::Water,   vc::blocks::Lava};
        for (size_t i = 0; i < std::size(kit); ++i) {
            m_inventory.Slot(i) = {kit[i], vc::kMaxStackSize};
        }
        // Debug kit (M26): buckets in the inventory grid (hotbar's full) —
        // an empty stack to test fill, plus a pre-filled water + lava bucket
        // to test dumping. Press E to grab them.
        m_inventory.Slot(9) = {vc::items::Bucket, vc::ItemMaxStack(vc::items::Bucket)};
        m_inventory.Slot(10) = {vc::items::WaterBucket, 1};
        m_inventory.Slot(11) = {vc::items::LavaBucket, 1};
        // M28 debug kit: all eight slabs/stairs so they're one E-press away to
        // test placement (half/facing/merge) and walking up them.
        const vc::BlockId shapes[] = {
            vc::blocks::StoneSlab,     vc::blocks::CobbleSlab,    vc::blocks::PlankSlab,
            vc::blocks::SandstoneSlab, vc::blocks::StoneStairs,   vc::blocks::CobbleStairs,
            vc::blocks::PlankStairs,   vc::blocks::SandstoneStairs};
        for (size_t i = 0; i < std::size(shapes); ++i) {
            m_inventory.Slot(12 + i) = {shapes[i], vc::kMaxStackSize};
        }
        // M33 debug kit: a full set of diamond armor in the grid so equipping
        // + damage reduction is one E-press away in a fresh world (every piece
        // is also in the creative palette for any world).
        for (int s = 0; s < vc::kArmorSlots; ++s) {
            m_inventory.Slot(20 + s) = {
                vc::items::ArmorPiece(vc::items::Diamond, static_cast<vc::ArmorSlot>(s)), 1};
        }
    }

    // M33 worn armor (absent in pre-armor saves -> nothing equipped).
    if (const auto& saved = m_world->SaveStore().GetArmor()) {
        for (const auto& s : *saved) {
            if (s.slot >= 0 && s.slot < vc::kArmorSlots && vc::IsArmor(s.id) && s.count > 0) {
                m_inventory.Armor(static_cast<size_t>(s.slot)) = {s.id, 1, std::max(s.damage, 0)};
            }
        }
    }

    // M30 vitals: restore from the save, or full health for a new/old world.
    if (const auto& v = m_world->SaveStore().GetVitals()) {
        m_player.SetVitals(v->health, v->foodLevel, v->saturation, v->exhaustion, v->air);
    }

    m_state = m_player.Dead() ? State::Dead : State::Playing;
    m_target.reset();
    m_sounds.ResetLocomotion(); // re-seed footstep tracking at the new spawn
    GetWindow().SetCursorCaptured(m_state != State::Dead);
    // Same guard as unpausing: buttons held through the menu click must be
    // re-pressed before they edit the world.
    m_input.breakWasDown = true;
    m_input.placeWasDown = true;
    m_input.breakCooldown = kEditRepeatDelay;
    m_input.placeCooldown = kEditRepeatDelay;
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
    m_sounds.StopAllFurnaceLoops();
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
    m_world->SaveStore().SetVitals({m_player.Health(), m_player.FoodLevel(),
                                    m_player.Saturation(), m_player.Exhaustion(),
                                    m_player.Air()});
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

    std::vector<vc::WorldSave::InventorySlot> armor;
    for (int s = 0; s < vc::kArmorSlots; ++s) {
        const vc::ItemStack& piece = m_inventory.Armor(static_cast<size_t>(s));
        if (!piece.Empty()) {
            armor.push_back({s, piece.id, piece.count, piece.damage});
        }
    }
    m_world->SaveStore().SetArmor(std::move(armor));
}

void GameApp::OnTick(double dt) {
    // Container screens don't pause the world (vanilla): block updates
    // run and the player keeps falling/floating, just without movement keys.
    if (m_world && (m_state == State::Playing || ContainerOpen())) {
        // M33: sum the worn-armor defense + toughness and feed it to the
        // player's damage calc before this tick's environmental damage runs.
        float armorDefense = 0.0f;
        float armorToughness = 0.0f;
        for (int s = 0; s < vc::kArmorSlots; ++s) {
            const vc::ItemStack& piece = m_inventory.Armor(static_cast<size_t>(s));
            if (!piece.Empty()) {
                armorDefense += static_cast<float>(vc::ArmorDefense(piece.id));
                armorToughness += vc::ArmorToughness(piece.id);
            }
        }
        m_player.SetArmorStats(armorDefense, armorToughness);

        m_player.Tick(*m_world, dt, m_state == State::Playing);
        if (m_player.ConsumeHurt()) {
            m_sounds.PlayHurt(); // M30: damage noise on each fresh hit
        }

        // Fly is creative (M30): no damage. Reused by the mob melee AND the M35
        // explosions (TNT in World::Tick, creeper in TickMobs).
        const auto damagePlayer = [this](float dmg, const glm::vec3& from, float kb) {
            if (m_player.GetMode() == Player::Mode::Walk) {
                m_player.Hurt(dmg, from, kb);
            }
        };
        // M35/M36: inject the player target (feet + AABB + damage callback)
        // before the world tick so a TNT detonation OR a mob-fired arrow (both
        // ticked in World::Tick) can hurt the player; World stays Player-agnostic
        // (the callback carries the dependency).
        m_world->Entities().SetEntityTargets(m_player.Position(), Player::kHalfWidth,
                                             Player::kHeight, damagePlayer);
        m_world->Tick(); // scheduled block updates (falling sand, water flow)

        // M32: tick mobs (AI/physics/combat/spawning). World stays Player- and
        // audio-free, so it gets the player state + callbacks and hands back
        // hurt/death sound events to play here.
        vc::EntityManager::MobTickCtx mobCtx;
        mobCtx.playerFeet = m_player.Position();
        mobCtx.playerHalfWidth = Player::kHalfWidth;
        mobCtx.playerHeight = Player::kHeight;
        mobCtx.isNight = IsNight();
        mobCtx.damagePlayer = damagePlayer; // zombie melee + creeper blast (gated above)
        mobCtx.pushPlayer = [&](float dx, float dz) { m_player.ExternalPush(*m_world, dx, dz); };
        m_world->Entities().TickMobs(mobCtx);
        if (m_player.ConsumeHurt()) {
            m_sounds.PlayHurt(); // a mob hit landed during the mob tick
        }
        for (const vc::MobSound& s : m_world->Entities().MobSoundEvents()) {
            switch (s.kind) {
            case 0: m_sounds.PlayMobHurt(s.type, s.pos); break;
            case 1: m_sounds.PlayMobDeath(s.type, s.pos); break;
            case 2: m_sounds.PlayChickenEgg(s.pos); break;    // M34: egg plop
            case 3: m_sounds.PlayCreeperPrime(s.pos); break;  // M35: creeper fuse hiss
            case 4: m_sounds.PlayBowShoot(s.pos); break;      // M36: skeleton bow shot
            default: break;
            }
        }
        m_world->Entities().MobSoundEvents().clear();

        // M35: explosions (TNT detonation in World::Tick + creeper detonation in
        // TickMobs) queued an event each — play the boom + spawn the debris puff.
        for (const auto& e : m_world->Entities().ExplosionEvents()) {
            m_sounds.PlayExplosion(e.pos);
            if (m_particles) {
                m_particles->SpawnExplosion(*m_world, e.pos, e.size);
            }
        }
        m_world->Entities().ExplosionEvents().clear();

        // M33: wear the worn pieces by the damage they absorbed this tick
        // (vanilla InventoryPlayer.damageArmor: max(1, raw/4) per piece);
        // a piece at its durability limit breaks and vanishes.
        if (const float wear = m_player.ConsumeArmorWear(); wear > 0.0f) {
            const int dmg = std::max(1, static_cast<int>(wear / 4.0f));
            for (int s = 0; s < vc::kArmorSlots; ++s) {
                vc::ItemStack& piece = m_inventory.Armor(static_cast<size_t>(s));
                const vc::ItemDef* def = vc::ItemRegistry::Get().Find(piece.id);
                if (!piece.Empty() && def && def->maxDamage > 0) {
                    piece.damage += dmg;
                    if (piece.damage >= def->maxDamage) {
                        piece = {};
                    }
                }
            }
        }

        m_particles->Tick(*m_world);
        m_viewModel->Tick(m_inventory.Slot(m_hotbarSlot));

        // Vacuum nearby drops: the player AABB grown by vanilla's pickup
        // reach (1.0, 0.5, 1.0); whatever the bag can't hold stays put.
        const glm::vec3 feet = m_player.Position();
        const glm::vec3 reach{Player::kHalfWidth + 1.0f, 0.5f, Player::kHalfWidth + 1.0f};
        bool pickedUp = false;
        m_world->Entities().PickupItems(
            feet - reach, feet + glm::vec3{0.0f, Player::kHeight, 0.0f} + reach,
            [&](uint16_t id, int count, int damage) {
                const vc::ItemStack leftover = m_inventory.Add({id, count, damage});
                const int taken = count - leftover.count;
                if (taken > 0) pickedUp = true;
                return taken;
            });
        // M36: collect stuck player-fired arrows (same grown pickup box).
        m_world->Entities().PickupArrows(
            feet - reach, feet + glm::vec3{0.0f, Player::kHeight, 0.0f} + reach, [&](int count) {
                const vc::ItemStack leftover = m_inventory.Add({vc::items::Arrow, count, 0});
                const int taken = count - leftover.count;
                if (taken > 0) pickedUp = true;
                return taken;
            });
        if (pickedUp) m_sounds.PlayPickup(); // one pop per tick max

        // M22: footsteps, landing thud, and a splash when entering water —
        // paced by GameSounds (it owns the stride + edge-tracking state).
        // Footsteps only while actually moving (Playing); landing/splash run
        // even in a container screen.
        m_sounds.UpdateLocomotion(*m_world, feet, m_player.Grounded(), m_player.InWater(),
                                  m_state == State::Playing);

        m_worldTime += 1.0;
        if (m_state == State::Playing && vox::Input::IsKeyDown(vox::Key::T)) {
            m_worldTime += kTimeFastForward; // debug: watch the cycle quickly
        }

        // M31: walk the debug Steve (only while actually playing).
        if (m_state == State::Playing) {
            TickDebugMob(dt);
        }

        // M30: health hit zero this tick — drop into the death screen.
        if (m_player.Dead() && m_state != State::Dead) {
            EnterDeathScreen();
        }
    }
    ++m_tickCount;
    ++m_totalTicks;
}

void GameApp::ToggleDebugMob() {
    if (!m_world) {
        return;
    }
    if (m_debugMob.active) {
        m_debugMob.active = false;
        GAME_INFO("Debug mob despawned");
        return;
    }
    // Spawn a circle ~4 blocks ahead of the player, on the surface there.
    glm::vec3 ahead = m_player.Position() + m_camera.Forward() * 4.0f;
    ahead.y = m_player.Position().y;
    m_debugMob = DebugMob{};
    m_debugMob.active = true;
    m_debugMob.center = {ahead.x, ahead.y, ahead.z};
    m_debugMob.pos = m_debugMob.center;
    m_debugMob.prevPos = m_debugMob.center;
    GAME_INFO("Debug mob spawned at ({:.1f}, {:.1f}, {:.1f})", ahead.x, ahead.y, ahead.z);
}

void GameApp::TickDebugMob(double dt) {
    if (!m_debugMob.active || !m_world) {
        return;
    }
    DebugMob& m = m_debugMob;
    m.prevPos = m.pos;
    m.prevYaw = m.yaw;
    m.prevLimbSwing = m.limbSwing;
    m.prevLimbSwingAmount = m.limbSwingAmount;

    // Pace a 2.5-block circle; face the direction of travel.
    constexpr float kRadius = 2.5f;
    constexpr float kOmega = 1.1f; // rad/s around the circle (~3 b/s tangential)
    m.angle += kOmega * static_cast<float>(dt);
    const float prevX = m.pos.x;
    const float prevZ = m.pos.z;
    m.pos.x = m.center.x + std::cos(m.angle) * kRadius;
    m.pos.z = m.center.z + std::sin(m.angle) * kRadius;
    m.yaw = std::atan2(m.pos.z - prevZ, m.pos.x - prevX);

    // Follow the surface: scan down from a little above the spawn height for
    // the first solid, non-liquid, non-plant block and stand the feet on top.
    const int cx = static_cast<int>(std::floor(m.pos.x));
    const int cz = static_cast<int>(std::floor(m.pos.z));
    const int top = static_cast<int>(std::floor(m.center.y)) + 4;
    for (int y = top; y > top - 24; --y) {
        const vc::BlockDef& def = vc::BlockRegistry::Get().Def(m_world->GetBlock(cx, y, cz));
        if (def.solid && !def.liquid && !def.cross) {
            m.pos.y = static_cast<float>(y + 1);
            break;
        }
    }

    // Vanilla EntityLivingBase walk-cycle accumulators from horizontal speed.
    const float dx = m.pos.x - m.prevPos.x;
    const float dz = m.pos.z - m.prevPos.z;
    float speed = std::sqrt(dx * dx + dz * dz) * 4.0f;
    if (speed > 1.0f) {
        speed = 1.0f;
    }
    m.limbSwingAmount += (speed - m.limbSwingAmount) * 0.4f;
    m.limbSwing += m.limbSwingAmount;
    m.age += 1.0f;
}

void GameApp::SpawnMobAhead(vc::MobType type, bool baby) {
    if (!m_world || m_state != State::Playing) {
        return;
    }
    // ~3 blocks ahead at eye-level x/z; it falls onto the ground via gravity.
    glm::vec3 ahead = m_player.Position() + m_camera.Forward() * 3.0f;
    ahead.y = m_player.Position().y + 1.0f;
    m_world->Entities().SpawnMob(type, ahead, baby);
    GAME_INFO("Spawned debug {}{} at ({:.1f}, {:.1f}, {:.1f})", baby ? "baby " : "",
              vc::MobSoundFolder(type), ahead.x, ahead.y, ahead.z);
}

float GameApp::BowDrawProgress() const {
    if (!m_bowDrawing) {
        return 0.0f;
    }
    // Vanilla ItemBow.getArrowVelocity: f = ticksHeld/20 (= seconds held), then
    // shaped (f*f + 2f)/3, clamped to 1 (full draw at ~1 s).
    float f = static_cast<float>(m_bowDrawSeconds);
    f = (f * f + 2.0f * f) / 3.0f;
    return std::min(f, 1.0f);
}

float GameApp::EatProgress() const {
    if (!m_eating) {
        return 0.0f;
    }
    return std::min(static_cast<float>(m_eatSeconds / kEatSeconds), 1.0f);
}

void GameApp::ReleaseBow() {
    if (!m_world) {
        return;
    }
    const float f = BowDrawProgress();
    if (f < 0.1f) {
        return; // vanilla: too short a draw fires nothing
    }
    const bool creative = m_player.GetMode() == Player::Mode::Fly;
    if (!creative) {
        // Spend one arrow from the bag (first match, hotbar-first like Add).
        bool consumed = false;
        for (size_t i = 0; i < vc::Inventory::kSize; ++i) {
            vc::ItemStack& s = m_inventory.Slot(i);
            if (!s.Empty() && s.id == vc::items::Arrow) {
                if (--s.count <= 0) {
                    s = {};
                }
                consumed = true;
                break;
            }
        }
        if (!consumed) {
            return; // no arrow (draw was gated on having one — defensive)
        }
    }

    const glm::vec3 eye = m_camera.Position();
    const glm::vec3 dir = m_camera.Forward();
    const float speed = f * 3.0f * 20.0f; // vanilla f*3.0 b/tick -> b/s
    const bool crit = f >= 1.0f;
    m_world->Entities().SpawnArrow(eye + dir * 0.3f, dir * speed,
                                   vc::EntityManager::ArrowOwner::Player, crit ? 2.5f : 2.0f,
                                   /*playerPickup=*/!creative);

    // Wear the bow one use (vanilla durability); creative doesn't wear.
    vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
    if (!creative && hand.id == vc::items::Bow) {
        const vc::ItemDef* def = vc::ItemRegistry::Get().Find(hand.id);
        if (def && def->maxDamage > 0 && ++hand.damage >= def->maxDamage) {
            hand = {};
        }
    }
    m_sounds.PlayBowShoot(eye);
    m_viewModel->TriggerSwing();
}

void GameApp::EmitEatChew(vc::ItemId food) {
    // Vanilla updateItemUse: a crunch + crumb burst every few ticks of chewing.
    // Crumbs fall from the mouth (just below/ahead of the eye), textured from
    // the food's sprite tile.
    m_sounds.PlayEat(m_camera.Position());
    if (m_particles && m_world) {
        const glm::vec3 mouth =
            m_camera.Position() + m_camera.Forward() * 0.4f + glm::vec3{0.0f, -0.25f, 0.0f};
        m_particles->SpawnEatCrumbs(*m_world, mouth, vc::ItemIconTile(food));
    }
}

void GameApp::EatHeldFood() {
    vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
    if (hand.Empty() || !vc::IsFood(hand.id)) {
        return; // defensive: hand changed mid-bite
    }
    const vc::ItemId id = hand.id;
    m_player.Eat(vc::FoodPoints(id), vc::FoodSaturation(id));
    m_sounds.PlayBurp(m_camera.Position()); // vanilla entity.player.burp on the last bite
    if (--hand.count <= 0) {
        hand = {};
    }
}

bool GameApp::IsNight() const {
    // Same phase as ComputeDayNight: night is when the sun is below the horizon
    // (elevation = sin(2*pi*t) < 0; t = 0 sunrise, 0.5 sunset).
    const double t = std::fmod(m_worldTime / kDayTicks, 1.0);
    return std::sin(static_cast<float>(t) * glm::two_pi<float>()) < 0.0f;
}

void GameApp::ThrowItem(const vc::ItemStack& stack) {
    if (!m_world || stack.Empty()) {
        return;
    }
    // Vanilla dropItem: from just below the eye, 0.3 b/tick (6 b/s) along
    // the look; 40-tick pickup delay so it isn't vacuumed straight back.
    const glm::vec3 dir = m_camera.Forward();
    const glm::vec3 origin = m_camera.Position() + glm::vec3{0.0f, -0.3f, 0.0f} + dir * 0.3f;
    m_world->Entities().SpawnItem(origin, dir * 6.0f, stack.id, stack.count, 40, stack.damage);
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
    m_input.breakWasDown = true;
    m_input.placeWasDown = true;
    m_input.breakCooldown = kEditRepeatDelay;
    m_input.placeCooldown = kEditRepeatDelay;
}

void GameApp::EnterDeathScreen() {
    // If a container was open when death landed (e.g. starving with the
    // inventory up), return its contents to the bag first, like CloseContainer.
    if (ContainerOpen()) {
        m_inventory.Add(m_carried);
        m_carried = {};
        for (vc::ItemStack& cell : m_craftGrid) {
            m_inventory.Add(cell);
            cell = {};
        }
    }
    m_state = State::Dead;
    GetWindow().SetCursorCaptured(false);
    m_target.reset();
    m_digCell.reset();
    m_digProgress = 0.0f;
}

void GameApp::RespawnPlayer() {
    // Respawn at the world spawn column, standing on the highest solid
    // (non-liquid) block. If that column isn't streamed in yet, fall from the
    // spawn height — the spawn fall-grace keeps the drop from hurting.
    glm::vec3 spawn = kSpawnPos;
    if (m_world) {
        const int x = static_cast<int>(std::floor(kSpawnPos.x));
        const int z = static_cast<int>(std::floor(kSpawnPos.z));
        for (int y = vc::World::kHeightChunks * 16 - 2; y >= 1; --y) {
            const vc::BlockDef& d = vc::BlockRegistry::Get().Def(m_world->GetBlock(x, y, z));
            if (d.solid && !d.liquid) {
                spawn = {kSpawnPos.x, static_cast<float>(y + 1), kSpawnPos.z};
                break;
            }
        }
    }
    m_player.Respawn(spawn);
    m_player.SetLook(kSpawnYaw, kSpawnPitch);
    m_state = State::Playing;
    GetWindow().SetCursorCaptured(true);
    m_target.reset();
    m_sounds.ResetLocomotion(); // re-seed footstep tracking at the spawn
    // Guard the click that hit Respawn from falling through into an edit.
    m_input.breakWasDown = true;
    m_input.placeWasDown = true;
    m_input.breakCooldown = kEditRepeatDelay;
    m_input.placeCooldown = kEditRepeatDelay;
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
        m_input.breakWasDown = true;
        m_input.placeWasDown = true;
        m_input.breakCooldown = kEditRepeatDelay;
        m_input.placeCooldown = kEditRepeatDelay;
    }
}

void GameApp::HandleInput(double frameDt, int scroll) {
    // F toggles walk/fly.
    const bool modeKey = vox::Input::IsKeyDown(vox::Key::F);
    if (modeKey && !m_input.modeKeyWasDown) {
        m_player.ToggleMode();
    }
    m_input.modeKeyWasDown = modeKey;

    // O toggles occlusion culling (debug/comparison; frustum-only when off).
    const bool occlusionKey = vox::Input::IsKeyDown(vox::Key::O);
    if (occlusionKey && !m_input.occlusionKeyWasDown) {
        m_occlusionCulling = !m_occlusionCulling;
        GAME_INFO("Occlusion culling {}", m_occlusionCulling ? "on" : "off");
    }
    m_input.occlusionKeyWasDown = occlusionKey;

    // G spawns / despawns the debug Steve (M31 box-model test entity).
    const bool mobKey = vox::Input::IsKeyDown(vox::Key::G);
    if (mobKey && !m_input.debugMobKeyWasDown) {
        ToggleDebugMob();
    }
    m_input.debugMobKeyWasDown = mobKey;

    // M32 debug spawns: B = pig, C = zombie, dropped ~3 blocks ahead so they're
    // easy to test without waiting for natural spawns.
    const bool pigKey = vox::Input::IsKeyDown(vox::Key::B);
    if (pigKey && !m_input.spawnPigKeyWasDown) {
        SpawnMobAhead(vc::MobType::Pig);
    }
    m_input.spawnPigKeyWasDown = pigKey;
    const bool zombieKey = vox::Input::IsKeyDown(vox::Key::C);
    if (zombieKey && !m_input.spawnZombieKeyWasDown) {
        SpawnMobAhead(vc::MobType::Zombie);
    }
    m_input.spawnZombieKeyWasDown = zombieKey;
    // M34 passive roster: V = cow, N = sheep, M = chicken.
    const bool cowKey = vox::Input::IsKeyDown(vox::Key::V);
    if (cowKey && !m_input.spawnCowKeyWasDown) {
        SpawnMobAhead(vc::MobType::Cow);
    }
    m_input.spawnCowKeyWasDown = cowKey;
    const bool sheepKey = vox::Input::IsKeyDown(vox::Key::N);
    if (sheepKey && !m_input.spawnSheepKeyWasDown) {
        SpawnMobAhead(vc::MobType::Sheep);
    }
    m_input.spawnSheepKeyWasDown = sheepKey;
    const bool chickenKey = vox::Input::IsKeyDown(vox::Key::M);
    if (chickenKey && !m_input.spawnChickenKeyWasDown) {
        SpawnMobAhead(vc::MobType::Chicken);
    }
    m_input.spawnChickenKeyWasDown = chickenKey;
    // M35: K = creeper.
    const bool creeperKey = vox::Input::IsKeyDown(vox::Key::K);
    if (creeperKey && !m_input.spawnCreeperKeyWasDown) {
        SpawnMobAhead(vc::MobType::Creeper);
    }
    m_input.spawnCreeperKeyWasDown = creeperKey;
    // M36: J = skeleton.
    const bool skeletonKey = vox::Input::IsKeyDown(vox::Key::J);
    if (skeletonKey && !m_input.spawnSkeletonKeyWasDown) {
        SpawnMobAhead(vc::MobType::Skeleton);
    }
    m_input.spawnSkeletonKeyWasDown = skeletonKey;
    // M38: H = a baby cow (test the half-scale render + grow-up; breeding itself
    // is tested by RMB-feeding two adults their breed item).
    const bool babyKey = vox::Input::IsKeyDown(vox::Key::H);
    if (babyKey && !m_input.spawnBabyKeyWasDown) {
        SpawnMobAhead(vc::MobType::Cow, /*baby=*/true);
    }
    m_input.spawnBabyKeyWasDown = babyKey;
    // M39: L = spider (climbs walls; aggressive only in the dark).
    const bool spiderKey = vox::Input::IsKeyDown(vox::Key::L);
    if (spiderKey && !m_input.spawnSpiderKeyWasDown) {
        SpawnMobAhead(vc::MobType::Spider);
    }
    m_input.spawnSpiderKeyWasDown = spiderKey;

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
    m_input.breakCooldown = std::max(0.0, m_input.breakCooldown - frameDt);
    m_input.placeCooldown = std::max(0.0, m_input.placeCooldown - frameDt);

    const bool breakDown = vox::Input::IsMouseButtonDown(vox::MouseButton::Left);

    // M32 melee: on the LMB press edge, if a mob is in reach and nearer than
    // any targeted block, attack it (with knockback) instead of digging.
    bool attackedMob = false;
    if (breakDown && !m_input.breakWasDown) {
        float mobDist = 0.0f;
        const auto mobHit =
            m_world->Entities().RaycastMob(m_camera.Position(), m_camera.Forward(), kReachDistance,
                                           mobDist);
        if (mobHit) {
            const float blockDist =
                m_target ? glm::length(m_target->point - m_camera.Position()) : 1e9f;
            if (mobDist <= blockDist) {
                vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
                float dmg = 2.0f; // bare hand (half-hearts*… tunable: no sword item yet)
                if (const vc::ItemDef* tool = vc::ItemRegistry::Get().Find(hand.id);
                    tool && tool->tool == vc::ToolClass::Axe) {
                    dmg += static_cast<float>(tool->tier) + 1.0f; // wood 1 .. iron 3 bonus
                }
                m_world->Entities().DamageMob(*mobHit, dmg, m_player.Position());
                m_viewModel->TriggerSwing();
                m_digCell.reset();
                m_digProgress = 0.0f;
                attackedMob = true;
            }
        }
    }

    if (attackedMob) {
        // Attack consumed the click; don't also break a block this press.
    } else if (m_player.GetMode() == Player::Mode::Fly) {
        // Creative-style (decided with the user): fly mode pops blocks
        // instantly and drops nothing — the palette is the source there.
        if (breakDown && m_target && (!m_input.breakWasDown || m_input.breakCooldown == 0.0)) {
            const vc::BlockId targetId =
                m_world->GetBlock(m_target->block.x, m_target->block.y, m_target->block.z);
            const vc::BlockDef& def = vc::BlockRegistry::Get().Def(targetId);
            if (!def.unbreakable) {
                m_world->SetBlock(m_target->block, vc::blocks::Air);
                m_particles->SpawnBlockDestroy(*m_world, m_target->block, targetId);
                m_sounds.PlayBreak(def.soundType, glm::vec3(m_target->block) + 0.5f);
                m_viewModel->TriggerSwing();
            }
            m_input.breakCooldown = kEditRepeatDelay;
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
                m_sounds.ResetDigSound(); // play a dig sound on the first hit
            }
            if (m_input.breakCooldown == 0.0) {
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
                m_sounds.TickDigSound(def.soundType, glm::vec3(m_target->block) + 0.5f, frameDt);
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
                    m_player.AddExhaustion(0.005f); // vanilla mining hunger cost
                    m_particles->SpawnBlockDestroy(*m_world, m_target->block, targetId);
                    m_sounds.PlayBreak(def.soundType, glm::vec3(m_target->block) + 0.5f);
                    if (canHarvest) {
                        m_world->Entities().SpawnBlockDrop(m_target->block,
                                                           def.ResolveDrop(targetId), 1);
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
                    m_input.breakCooldown = kEditRepeatDelay; // vanilla blockHitDelay (5 ticks)
                }
            }
        }
    } else {
        m_digCell.reset();
        m_digProgress = 0.0f;
    }
    m_input.breakWasDown = breakDown;

    // Q tosses one item from the hand (press, then repeat while held).
    m_input.dropCooldown = std::max(0.0, m_input.dropCooldown - frameDt);
    const bool dropKey = vox::Input::IsKeyDown(vox::Key::Q);
    if (dropKey && (!m_input.dropKeyWasDown || m_input.dropCooldown == 0.0)) {
        vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
        if (!hand.Empty()) {
            ThrowItem({hand.id, 1, hand.damage});
            if (--hand.count <= 0) {
                hand = {};
            }
            m_input.dropCooldown = kEditRepeatDelay;
        }
    }
    m_input.dropKeyWasDown = dropKey;

    const bool placeDown = vox::Input::IsMouseButtonDown(vox::MouseButton::Right);

    // M36 bow: RMB-hold draws (charges) instead of placing; release fires. Gated
    // ahead of the place chain — a drawn bow consumes RMB entirely. Drawing needs
    // an arrow in the bag (free in fly/creative).
    {
        const vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
        if (!hand.Empty() && hand.id == vc::items::Bow) {
            const bool creative = m_player.GetMode() == Player::Mode::Fly;
            bool hasArrow = creative;
            for (size_t i = 0; i < vc::Inventory::kSize && !hasArrow; ++i) {
                const vc::ItemStack& s = m_inventory.Slot(i);
                hasArrow = !s.Empty() && s.id == vc::items::Arrow;
            }
            if (placeDown && hasArrow) {
                m_bowDrawing = true;
                m_bowDrawSeconds += frameDt;
            } else {
                if (m_bowDrawing && !placeDown) {
                    ReleaseBow(); // fire on release (scaled by how long it charged)
                }
                m_bowDrawing = false;
                m_bowDrawSeconds = 0.0;
            }
            m_input.placeWasDown = placeDown;
            return; // bow owns RMB this frame; skip the place chain
        }
        // Switched off the bow mid-draw: cancel without firing.
        m_bowDrawing = false;
        m_bowDrawSeconds = 0.0;
    }

    // M37 eat: RMB-hold a food item to eat it (vanilla 32-tick use). Owns RMB
    // only while actually eating, so right-clicking a block with food in hand
    // at full hunger still uses the block (canEat gates it out, falls through).
    // Walk-only — fly is creative, no hunger to refill.
    {
        const vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
        const bool eating = !hand.Empty() && vc::IsFood(hand.id) &&
                            m_player.GetMode() == Player::Mode::Walk &&
                            m_player.CanEat(vc::AlwaysEdible(hand.id)) && placeDown;
        if (eating) {
            if (!m_eating) {
                m_eating = true;
                m_eatSeconds = 0.0;
                m_eatChewAccum = 0.0;
            }
            m_eatSeconds += frameDt;
            m_eatChewAccum += frameDt;
            if (m_eatChewAccum >= kEatChewInterval) {
                m_eatChewAccum -= kEatChewInterval;
                EmitEatChew(hand.id); // crunch sound + crumb particles
            }
            if (m_eatSeconds >= kEatSeconds) {
                EatHeldFood(); // consume one, restore hunger, burp
                m_eating = false;
                m_eatSeconds = 0.0;
            }
            m_input.placeWasDown = placeDown;
            return; // eating owns RMB this frame; skip the place chain
        }
        // Released, switched away, or hunger full: cancel any in-progress eat.
        m_eating = false;
        m_eatSeconds = 0.0;
        m_eatChewAccum = 0.0;
    }

    if (placeDown && (!m_input.placeWasDown || m_input.placeCooldown == 0.0)) {
        const vc::BlockId targetId =
            m_target ? m_world->GetBlock(m_target->block.x, m_target->block.y, m_target->block.z)
                     : vc::blocks::Air;
        if (TryShearSheep()) {
            // Shears + sheep beats placing; it does its own mob raycast.
            m_input.placeCooldown = kEditRepeatDelay;
        } else if (TryIgnite()) {
            // M35: flint & steel priming a TNT block / igniting a creeper beats
            // placing; it does its own mob + block-target checks (+ its cooldown).
        } else if (TryFeedMob()) {
            // M38: feeding an animal its breed item (wheat/carrot/seeds) beats
            // placing; it does its own mob raycast + consume.
            m_input.placeCooldown = kEditRepeatDelay;
        } else if (m_target && targetId == vc::blocks::CraftingTable) {
            // Use beats place (vanilla, sans sneak): open the 3x3 grid.
            OpenContainer(State::Crafting);
            m_input.placeCooldown = kEditRepeatDelay;
        } else if (m_target &&
                   (targetId == vc::blocks::Furnace || targetId == vc::blocks::LitFurnace)) {
            m_openFurnace = m_target->block;
            OpenContainer(State::Furnace);
            m_input.placeCooldown = kEditRepeatDelay;
        } else if (TryUseBucket()) {
            // Bucket fill/dump (or a no-op bucket click) — handled, no place.
            // It does its own liquid raycast, so it works with no crosshair
            // block target (aiming straight at water).
        } else if (m_target) {
            vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
            if (!hand.Empty() && vc::IsBlockItem(hand.id)) {
                const auto handId = static_cast<vc::BlockId>(hand.id);
                const vc::BlockDef& def = vc::BlockRegistry::Get().Def(handId);

                glm::ivec3 placePos = m_target->block + m_target->normal;
                vc::BlockId placeId = handId;
                uint8_t placeMeta = 0;
                bool allowed = false;

                // M28 double-slab merge: clicking the matching slab so the new
                // one completes the cell turns it into the full base block,
                // in place, instead of placing a second slab in the neighbor.
                const bool mergeSlab =
                    def.slab && m_target->normal.y != 0 && targetId == handId &&
                    (vc::facing::SlabIsTop(m_world->GetMeta(
                         m_target->block.x, m_target->block.y, m_target->block.z))
                         ? m_target->normal.y < 0
                         : m_target->normal.y > 0);
                if (mergeSlab) {
                    placePos = m_target->block;
                    placeId = def.slabBase;
                    placeMeta = 0;
                    allowed = true;
                } else if (!m_world->IsSolid(placePos.x, placePos.y, placePos.z) &&
                           !m_player.Intersects(placePos)) {
                    allowed = true;
                    // M24/M28 orientation: torches mount on the clicked surface;
                    // furnace/table fronts point back at the placer; slabs/stairs
                    // read the clicked half + (stairs) the look direction.
                    if (def.torch) {
                        const glm::ivec3 n = m_target->normal;
                        if (n.y > 0) {
                            allowed = m_world->IsSolid(placePos.x, placePos.y - 1, placePos.z);
                            placeMeta = vc::facing::TorchFloor;
                        } else if (n.y == 0) {
                            const vc::BlockFace f = n.x > 0   ? vc::BlockFace::PosX
                                                    : n.x < 0 ? vc::BlockFace::NegX
                                                    : n.z > 0 ? vc::BlockFace::PosZ
                                                              : vc::BlockFace::NegZ;
                            allowed = m_world->IsSolid(m_target->block.x, m_target->block.y,
                                                       m_target->block.z);
                            placeMeta = vc::facing::TorchWallMeta(f);
                        } else {
                            allowed = false; // bottom face: nothing to hang from
                        }
                    } else if (def.horizontalFacing) {
                        placeMeta = static_cast<uint8_t>(vc::facing::Opposite(
                            vc::facing::HorizontalFromLook(m_camera.Forward())));
                    } else if (def.slab) {
                        placeMeta = PlaceTopHalf(m_target->normal, m_target->point)
                                        ? vc::facing::SlabTopMeta
                                        : vc::facing::SlabBottom;
                    } else if (def.stairs) {
                        placeMeta = vc::facing::StairsMeta(
                            vc::facing::HorizontalFromLook(m_camera.Forward()),
                            PlaceTopHalf(m_target->normal, m_target->point));
                    }
                }

                if (allowed) {
                    m_world->SetBlock(placePos, placeId, placeMeta);
                    m_sounds.PlayPlace(def.soundType, glm::vec3(placePos) + 0.5f);
                    m_viewModel->TriggerSwing();
                    if (--hand.count <= 0) {
                        hand = {};
                    }
                    m_input.placeCooldown = kEditRepeatDelay;
                }
            }
        }
    }
    m_input.placeWasDown = placeDown;
}

bool GameApp::TryUseBucket() {
    vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
    if (hand.Empty()) {
        return false;
    }
    if (hand.id == vc::items::Bucket) {
        // Fill: aim at a liquid SOURCE (the bucket's own ray sees liquids;
        // the crosshair ray skips them). Flowing liquid can't be scooped.
        const auto hit = m_world->RaycastBlocks(m_camera.Position(), m_camera.Forward(),
                                                kReachDistance, /*includeLiquids=*/true);
        if (hit) {
            const vc::BlockId at = m_world->GetBlock(hit->block.x, hit->block.y, hit->block.z);
            const vc::ItemId filled = vc::items::FilledBucketFor(at);
            if (filled != 0) {
                m_world->SetBlock(hit->block, vc::blocks::Air);
                m_sounds.PlayBucketFill(at == vc::blocks::Lava, glm::vec3(hit->block) + 0.5f);
                m_viewModel->TriggerSwing();
                // One empty bucket becomes the filled one; from a larger
                // stack, peel one off and stow the fill (toss if no room).
                if (--hand.count <= 0) {
                    hand = {filled, 1};
                } else if (vc::ItemStack leftover = m_inventory.Add({filled, 1});
                           !leftover.Empty()) {
                    ThrowItem(leftover);
                }
                m_input.placeCooldown = kEditRepeatDelay;
            }
        }
        return true; // holding a bucket: this click is a bucket action, not a place
    }
    const vc::BlockId liquid = vc::items::BucketLiquid(hand.id);
    if (liquid == vc::blocks::Air) {
        return false; // not a bucket
    }
    // Dump: place the source against the aimed face, like placing a block.
    if (m_target) {
        const glm::ivec3 cell = m_target->block + m_target->normal;
        if (!m_world->IsSolid(cell.x, cell.y, cell.z) && !m_player.Intersects(cell)) {
            m_world->SetBlock(cell, liquid);
            m_sounds.PlayBucketEmpty(liquid == vc::blocks::Lava, glm::vec3(cell) + 0.5f);
            m_viewModel->TriggerSwing();
            hand = {vc::items::Bucket, 1};
            m_input.placeCooldown = kEditRepeatDelay;
        }
    }
    return true;
}

bool GameApp::TryShearSheep() {
    vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
    if (hand.id != vc::items::Shears) {
        return false; // not holding shears -> normal RMB place/use
    }
    float mobDist = 0.0f;
    const auto mobHit = m_world->Entities().RaycastMob(m_camera.Position(), m_camera.Forward(),
                                                       kReachDistance, mobDist);
    if (!mobHit) {
        return false;
    }
    // A block target nearer than the mob wins (you can still shear-place... no:
    // shears never place, so just don't shear through a wall).
    const float blockDist =
        m_target ? glm::length(m_target->point - m_camera.Position()) : 1e9f;
    if (mobDist > blockDist) {
        return false;
    }
    const glm::vec3 mobPos = m_world->Entities().Mobs()[*mobHit].pos;
    const int wool = m_world->Entities().ShearMob(*mobHit);
    if (wool <= 0) {
        return false; // not a sheep, or already sheared -> let RMB fall through
    }
    // Grant the wool straight to the bag (toss any overflow at the feet).
    if (vc::ItemStack leftover = m_inventory.Add({vc::blocks::WhiteWool, wool});
        !leftover.Empty()) {
        ThrowItem(leftover);
    }
    m_sounds.PlaySheepShear(mobPos);
    m_viewModel->TriggerSwing();
    // Wear the shears one use (vanilla); break it at the durability limit.
    if (const vc::ItemDef* def = vc::ItemRegistry::Get().Find(hand.id);
        def && def->maxDamage > 0) {
        if (++hand.damage >= def->maxDamage) {
            hand = {};
        }
    }
    return true;
}

bool GameApp::TryIgnite() {
    vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
    if (hand.id != vc::items::FlintAndSteel) {
        return false; // not holding flint & steel -> normal RMB place/use
    }
    // Wear one use + break at the durability limit (shared by both ignite paths).
    const auto wear = [&] {
        if (const vc::ItemDef* def = vc::ItemRegistry::Get().Find(hand.id);
            def && def->maxDamage > 0) {
            if (++hand.damage >= def->maxDamage) {
                hand = {};
            }
        }
    };

    // A creeper in reach (and nearer than any block target) gets force-ignited.
    float mobDist = 0.0f;
    const auto mobHit = m_world->Entities().RaycastMob(m_camera.Position(), m_camera.Forward(),
                                                       kReachDistance, mobDist);
    const float blockDist =
        m_target ? glm::length(m_target->point - m_camera.Position()) : 1e9f;
    if (mobHit && mobDist <= blockDist) {
        const glm::vec3 mobPos = m_world->Entities().Mobs()[*mobHit].pos;
        if (m_world->Entities().IgniteMob(*mobHit)) {
            m_sounds.PlayCreeperPrime(mobPos);
            m_viewModel->TriggerSwing();
            wear();
            m_input.placeCooldown = kEditRepeatDelay;
            return true;
        }
        return false; // a non-creeper mob is in front -> nothing to ignite
    }

    // Otherwise, prime a targeted TNT block into a falling/primed entity.
    if (m_target) {
        const vc::BlockId targetId =
            m_world->GetBlock(m_target->block.x, m_target->block.y, m_target->block.z);
        if (targetId == vc::blocks::Tnt) {
            const glm::vec3 center = glm::vec3(m_target->block) + 0.5f;
            m_world->SetBlock(m_target->block, vc::blocks::Air);
            m_world->Entities().SpawnPrimedTnt(center, 80); // vanilla 4 s fuse
            m_sounds.PlayCreeperPrime(center);              // the fuse hiss doubles as the ignite
            m_viewModel->TriggerSwing();
            wear();
            m_input.placeCooldown = kEditRepeatDelay;
            return true;
        }
    }
    return false;
}

bool GameApp::TryFeedMob() {
    vc::ItemStack& hand = m_inventory.Slot(m_hotbarSlot);
    if (hand.Empty()) {
        return false;
    }
    // Fast out: only the three breed items can feed anything (mirrors how
    // TryShearSheep/TryIgnite gate on the held tool before raycasting).
    if (hand.id != vc::items::Wheat && hand.id != vc::items::Carrot &&
        hand.id != vc::items::Seeds) {
        return false;
    }
    float mobDist = 0.0f;
    const auto mobHit = m_world->Entities().RaycastMob(m_camera.Position(), m_camera.Forward(),
                                                       kReachDistance, mobDist);
    if (!mobHit) {
        return false;
    }
    // Don't feed through a wall: a nearer block target wins.
    const float blockDist =
        m_target ? glm::length(m_target->point - m_camera.Position()) : 1e9f;
    if (mobDist > blockDist) {
        return false;
    }
    const glm::vec3 mobPos = m_world->Entities().Mobs()[*mobHit].pos;
    if (!m_world->Entities().FeedMob(*mobHit, hand.id)) {
        return false; // wrong animal, or it's not receptive -> RMB falls through
    }
    // Accepted: consume one, swing, and play the eat munch at the animal.
    m_sounds.PlayEat(mobPos);
    m_viewModel->TriggerSwing();
    if (--hand.count <= 0) {
        hand = {};
    }
    return true;
}

const vc::BlockDef& GameApp::EyeLiquid() const {
    static const vc::BlockDef kAir; // air: liquid=false, liquidSource=0
    if (!m_world) {
        return kAir;
    }
    const glm::vec3 eye = m_camera.Position();
    return vc::BlockRegistry::Get().Def(
        m_world->GetBlock(static_cast<int>(std::floor(eye.x)), static_cast<int>(std::floor(eye.y)),
                          static_cast<int>(std::floor(eye.z))));
}

bool GameApp::EyeInWater() const { return EyeLiquid().liquidSource == vc::blocks::Water; }

bool GameApp::EyeInLava() const { return EyeLiquid().liquidSource == vc::blocks::Lava; }

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
    const bool clicked = clickDown && !m_input.clickWasDown;
    m_input.clickWasDown = clickDown;
    const bool rightDown = vox::Input::IsMouseButtonDown(vox::MouseButton::Right);
    const bool rightClicked = rightDown && !m_input.rightClickWasDown;
    m_input.rightClickWasDown = rightDown;

    const glm::vec2 mouse = vox::Input::MousePosition();

    // Bake (or re-bake on a GUI-scale change) the 3D block-icon sheet before
    // the 2D batch; restores the window framebuffer + viewport itself.
    m_blockIcons->EnsureBuilt(static_cast<int>(16.0f * vc::GuiScale(screen)),
                              GetWindow().Width(), GetWindow().Height());

    // M33: bake the player doll (body + worn armor) while depth is still on,
    // then restore the window framebuffer + viewport (like EnsureBuilt). Only
    // the player inventory shows it; other container screens leave it null.
    m_guiTextures.playerDoll = nullptr;
    if (m_state == State::Inventory && m_playerDoll) {
        const float gs = vc::GuiScale(screen);
        const auto* doll = m_playerDoll->Bake(
            *m_entityModelShader, static_cast<int>(vc::InventoryScreen::kDollBoxSize.x * gs),
            static_cast<int>(vc::InventoryScreen::kDollBoxSize.y * gs), m_inventory.ArmorSlots(),
            static_cast<float>(m_totalTicks));
        if (doll) {
            m_guiTextures.playerDoll = *doll;
        }
        vox::Renderer::SetViewport(GetWindow().Width(), GetWindow().Height());
    }

    m_ui->Begin(GetWindow().Width(), GetWindow().Height(), m_blockTextures.get());
    if (EyeInLava()) {
        // Submerged-in-lava tint: thick orange, almost blinding (vanilla).
        m_ui->DrawRect({0.0f, 0.0f}, screen, {0.78f, 0.24f, 0.04f, 0.72f});
    } else if (EyeInWater()) {
        // Underwater tint on top of the shader fog.
        m_ui->DrawRect({0.0f, 0.0f}, screen, {0.09f, 0.27f, 0.55f, 0.35f});
    } else if (m_player.OnFire()) {
        if (m_fireOverlay) {
            // Real vanilla fire: the blocks/fire_layer_1 animation tiled across
            // the bottom of the view, flames licking upward (ItemRenderer.
            // renderFireInFirstPerson draws it at alpha 0.9). 32 frames of
            // 16x16 stacked in a 16x512 strip; columns desynced so they don't
            // pulse in unison.
            constexpr int kFrames = 32;
            const float flameH = screen.y * 0.45f;
            const int cols = static_cast<int>(std::ceil(screen.x / flameH)) + 1;
            for (int i = 0; i < cols; ++i) {
                const int frame =
                    static_cast<int>((m_totalTicks + static_cast<uint64_t>(i * 7)) % kFrames);
                m_ui->DrawImage(m_fireOverlay,
                                {static_cast<float>(i) * flameH, screen.y - flameH},
                                {flameH, flameH}, {0.0f, static_cast<float>(frame * 16)},
                                {16.0f, 16.0f}, {1.0f, 1.0f, 1.0f, 0.9f});
            }
        } else {
            // No overlay asset: fall back to a flickering orange tint.
            const float flicker = 0.18f + 0.07f * std::sin(static_cast<float>(m_worldTime) * 1.7f);
            m_ui->DrawRect({0.0f, screen.y * 0.5f}, {screen.x, screen.y * 0.5f},
                           {0.85f, 0.35f, 0.05f, flicker});
        }
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
        const vc::HudVitals vitals{m_player.Health(), m_player.FoodLevel(), m_player.Air(),
                                   m_player.GetMode() == Player::Mode::Walk};
        vc::Hud::Draw(*m_ui, screen, m_inventory.Hotbar(), m_hotbarSlot, m_guiTextures, vitals);
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
                    table ? 3 : 2, m_carried, thrown, m_guiTextures, m_paletteScroll);
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
        } else if (m_state == State::Dead) {
            // Vanilla's red "You Died!" overlay with respawn / quit buttons.
            m_ui->DrawRect({0.0f, 0.0f}, screen, {0.5f, 0.0f, 0.0f, 0.4f});
            const float s = vc::GuiScale(screen);
            const float titleScale = vc::UiTextScale(*m_ui, s) + 1.0f;
            constexpr std::string_view kTitle = "You Died!";
            const glm::vec2 titleSize = m_ui->MeasureText(kTitle, titleScale);
            vc::ShadowedText(
                *m_ui,
                glm::floor(glm::vec2((screen.x - titleSize.x) * 0.5f, screen.y * 0.3f)),
                kTitle, titleScale);
            const glm::vec2 buttonSize{200.0f * s, 20.0f * s};
            const float bx = std::floor((screen.x - buttonSize.x) * 0.5f);
            const float by = std::floor(screen.y * 0.45f);
            const float gap = 6.0f * s;
            if (vc::UiButton(*m_ui, s, {bx, by}, buttonSize, "Respawn", mouse, clicked,
                             m_guiTextures.widgets)) {
                RespawnPlayer();
            }
            if (vc::UiButton(*m_ui, s, {bx, by + buttonSize.y + gap}, buttonSize, "Title Screen",
                             mouse, clicked, m_guiTextures.widgets)) {
                ExitToTitle();
            }
        }
    }
    m_ui->End();
}

void GameApp::OnRender(double alpha, double frameDt) {
    const bool escapeDown = vox::Input::IsKeyDown(vox::Key::Escape);
    if (escapeDown && !m_input.escapeWasDown) {
        if (ContainerOpen()) {
            CloseContainer();
        } else {
            SetPaused(m_state == State::Playing); // no-op on the title screen
        }
    }
    m_input.escapeWasDown = escapeDown;

    // E opens the inventory / closes any container screen.
    const bool inventoryKey = vox::Input::IsKeyDown(vox::Key::E);
    if (inventoryKey && !m_input.inventoryKeyWasDown) {
        if (ContainerOpen()) {
            CloseContainer();
        } else {
            OpenContainer(State::Inventory);
        }
    }
    m_input.inventoryKeyWasDown = inventoryKey;

    // Consume wheel input every frame so clicks made over menus don't
    // burst-apply to the hotbar on resume.
    const int scroll = static_cast<int>(GetWindow().TakeScrollY());
    // While the inventory screen is open the wheel scrolls the creative palette
    // (DrawUi clamps the offset to the row count). Wheel-up shows earlier rows.
    if (m_state == State::Inventory && scroll != 0) {
        m_paletteScroll -= scroll;
    }

    if (m_world) {
        const bool playing = m_state == State::Playing;
        m_player.OnRender(alpha, playing);

        // M30 hurt/death camera tilt (vanilla hurtCameraEffect): a transient
        // roll from the last hit, plus the slow keel-over while dead.
        m_deathAnim = (m_state == State::Dead) ? m_deathAnim + frameDt : 0.0;
        float roll = m_player.CameraRoll(alpha);
        if (m_state == State::Dead) {
            const float deathTicks = static_cast<float>(m_deathAnim * 20.0);
            roll += 40.0f - 8000.0f / (deathTicks + 200.0f);
        }
        m_camera.SetRoll(roll);

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
            // Keep one crackle loop per lit furnace (GameSounds owns the voices).
            m_sounds.ReconcileFurnaceLoops(*m_world);

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
        if (EyeInLava()) {
            // Dense, glowing-orange fog — you can barely see in lava (vanilla).
            m_chunkShader->SetFloat3("u_fogColor", {0.60f, 0.18f, 0.02f});
            m_chunkShader->SetFloat2("u_fogRange", {0.25f, 2.5f});
        } else if (EyeInWater()) {
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
            if (EyeInLava()) {
                m_modelShader->SetFloat3("u_fogColor", {0.60f, 0.18f, 0.02f});
                m_modelShader->SetFloat2("u_fogRange", {0.25f, 2.5f});
            } else if (EyeInWater()) {
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
        const auto& fallingBlocks = m_world->Entities().FallingBlocks();
        const auto& items = m_world->Entities().ItemEntities();
        const auto& primedTnt = m_world->Entities().PrimedTnts();
        const int crackStage =
            m_digCell ? std::min(static_cast<int>(m_digProgress * 10.0f) - 1, 9) : -1;
        if (!fallingBlocks.empty() || !items.empty() || !primedTnt.empty() || crackStage >= 0) {
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
                if (!m_world->Entities().FallingBlockVisible(falling)) {
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

            // M35: primed TNT — a near-full cube that blinks bright (u_unlit) in
            // its final ticks, vanilla's white flash (fuse/5 % 2). Reset u_unlit
            // afterwards so the item loop below renders lit.
            for (const auto& tnt : primedTnt) {
                const glm::vec3 pos =
                    glm::mix(tnt.prevPos, tnt.pos, static_cast<float>(alpha));
                setFaceLayers(vc::BlockRegistry::Get().Def(vc::blocks::Tnt).faceTiles);
                setCellLight({static_cast<int>(std::floor(pos.x)),
                              static_cast<int>(std::floor(pos.y + 0.5f)),
                              static_cast<int>(std::floor(pos.z))});
                const float fuseF =
                    glm::mix(static_cast<float>(tnt.prevFuse), static_cast<float>(tnt.fuse),
                             static_cast<float>(alpha));
                const bool flash = (static_cast<int>(fuseF) / 5) % 2 == 0;
                m_entityShader->SetFloat("u_unlit", flash ? 1.0f : 0.0f);
                m_entityShader->SetFloat3("u_center", pos + glm::vec3{0.0f, 0.49f, 0.0f});
                m_entityShader->SetFloat("u_scale", 0.98f);
                m_entityShader->SetFloat("u_yaw", 0.0f);
                vox::Renderer::DrawIndexed(*m_entityCube);
            }
            m_entityShader->SetFloat("u_unlit", 0.0f); // restore for the item/crack passes

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

        // M31: the debug Steve (jointed box model). Opaque + alpha-tested, so
        // it draws in the opaque slot before the blended water pass.
        if (m_debugMob.active && m_humanoid && m_humanoid->Ready()) {
            const float a = static_cast<float>(alpha);
            const glm::vec3 pos = glm::mix(m_debugMob.prevPos, m_debugMob.pos, a);
            const float limbSwing = glm::mix(m_debugMob.prevLimbSwing, m_debugMob.limbSwing, a);
            const float limbAmount =
                glm::mix(m_debugMob.prevLimbSwingAmount, m_debugMob.limbSwingAmount, a);
            m_humanoid->SetRotationAngles(limbSwing, limbAmount, m_debugMob.age + a, 0.0f, 0.0f);

            // Sample light at the body cell (like the entity-cube path).
            const glm::ivec3 cell{static_cast<int>(std::floor(pos.x)),
                                  static_cast<int>(std::floor(pos.y)) + 1,
                                  static_cast<int>(std::floor(pos.z))};
            const uint8_t packed = m_world->PackedLightAt(cell);

            m_entityModelShader->Bind();
            m_entityModelShader->SetMat4("u_viewProj", viewProj);
            m_entityModelShader->SetInt("u_skin", 1);
            m_entityModelShader->SetFloat3("u_sunDir", dayNight.lightDir);
            m_entityModelShader->SetFloat("u_sunLight", dayNight.sunLight);
            m_entityModelShader->SetFloat3("u_skyTint", dayNight.skyTint);
            m_entityModelShader->SetFloat2(
                "u_light", {static_cast<float>(vc::ChunkLight::Sky(packed)) / 15.0f,
                            static_cast<float>(vc::ChunkLight::Block(packed)) / 15.0f});
            m_entityModelShader->SetFloat("u_hurt", 0.0f); // debug Steve never flashes

            // Pixel/Y-down model space -> world: place feet at pos, face the
            // travel direction, scale 1/16, then flip upright (Rx(pi) + the
            // 24px feet offset). The model looks toward +Z after the flip, so
            // the body yaw is (pi/2 - travelYaw).
            constexpr float kPi = 3.14159265358979323846f;
            glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
            m = glm::rotate(m, kPi * 0.5f - m_debugMob.yaw, {0.0f, 1.0f, 0.0f});
            m = glm::scale(m, glm::vec3(1.0f / 16.0f));
            m = glm::translate(m, {0.0f, 24.0f, 0.0f});
            m = glm::rotate(m, kPi, {1.0f, 0.0f, 0.0f});

            vox::Renderer::SetCullFace(false);
            m_humanoid->Render(*m_entityModelShader, m);
            vox::Renderer::SetCullFace(true);
            m_chunkShader->Bind();
        }

        // M32: living mobs (pigs/zombies). Same opaque slot + box-model shader
        // as the debug Steve; one bind, then per-mob articulation + matrix.
        const auto& mobs = m_world->Entities().Mobs();
        if (!mobs.empty()) {
            constexpr float kPi = 3.14159265358979323846f;
            const float a = static_cast<float>(alpha);
            m_entityModelShader->Bind();
            m_entityModelShader->SetMat4("u_viewProj", viewProj);
            m_entityModelShader->SetInt("u_skin", 1);
            m_entityModelShader->SetFloat3("u_sunDir", dayNight.lightDir);
            m_entityModelShader->SetFloat("u_sunLight", dayNight.sunLight);
            m_entityModelShader->SetFloat3("u_skyTint", dayNight.skyTint);
            vox::Renderer::SetCullFace(false);
            for (const vc::Mob& mob : mobs) {
                vc::IMobModel* model = m_mobModels[static_cast<size_t>(mob.type)].get();
                if (!model || !model->Ready()) {
                    continue; // no skin overlay -> draw nothing (like the bare arm)
                }
                const vc::MobDef& def = vc::MobDefOf(mob.type);
                const glm::vec3 pos = glm::mix(mob.prevPos, mob.pos, a);
                const float yaw = glm::mix(mob.prevYaw, mob.yaw, a);
                const float limbSwing = glm::mix(mob.prevLimbSwing, mob.limbSwing, a);
                const float limbAmount =
                    glm::mix(mob.prevLimbSwingAmount, mob.limbSwingAmount, a);

                // Light at the body-center cell (like the entity-cube path).
                const glm::ivec3 cell{static_cast<int>(std::floor(pos.x)),
                                      static_cast<int>(std::floor(pos.y + def.height * 0.5f)),
                                      static_cast<int>(std::floor(pos.z))};
                const uint8_t packed = m_world->PackedLightAt(cell);
                m_entityModelShader->SetFloat2(
                    "u_light", {static_cast<float>(vc::ChunkLight::Sky(packed)) / 15.0f,
                                static_cast<float>(vc::ChunkLight::Block(packed)) / 15.0f});
                m_entityModelShader->SetFloat("u_hurt", mob.hurtTime > 0 ? 1.0f : 0.0f);

                // Pixel/Y-down model -> world: feet at pos, facing travel/target,
                // 1/16 * modelScale, then the upright flip (Rx(pi) + the feet
                // offset). modelScale folds in around the feet (offset is applied
                // after the scale) so a scaled mob still stands on the ground —
                // the hook baby/variant sizing will use (all 1.0 today).
                // M38 babies render at half scale (folded into the same feet-
                // anchored scale as modelScale, so the offset translate after it
                // keeps the smaller body standing on the ground).
                const float scale =
                    def.modelScale * (mob.baby ? vc::kBabyModelScale : 1.0f);
                glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
                m = glm::rotate(m, kPi * 0.5f - yaw, {0.0f, 1.0f, 0.0f});
                m = glm::scale(m, glm::vec3(scale / 16.0f));
                m = glm::translate(m, {0.0f, def.modelOffsetPx, 0.0f});
                m = glm::rotate(m, kPi, {1.0f, 0.0f, 0.0f});

                // Per-mob render variant: sheep toggle their wool (sheared),
                // skeletons raise the bow-aim arms while drawing (M36 aiming).
                int variant = 0;
                if (mob.type == vc::MobType::Sheep) {
                    variant = mob.sheared ? 1 : 0;
                } else if (mob.type == vc::MobType::Skeleton) {
                    variant = mob.aiming ? 1 : 0;
                }
                model->SetVariant(variant);
                model->SetRotationAngles(limbSwing, limbAmount, mob.age + a, 0.0f, 0.0f);
                model->Render(*m_entityModelShader, m);

                // M36: the bow in the skeleton's right hand. RightArmTransform
                // gives the arm joint's world frame, so (0,9,0) in arm-local
                // pixels is the hand — we read just that world POSITION from it
                // (it tracks the pose), then build the bow's orientation directly
                // in world space: a vertical quad facing the aim direction, a
                // little under a block tall. Reasoning the orientation out through
                // the Y-down + upright-flip + arm-rotation chain is error-prone;
                // a world-space basis is predictable and reads correctly.
                if (mob.type == vc::MobType::Skeleton && m_heldBow && m_heldBow->Ready()) {
                    auto* biped = static_cast<vc::HumanoidModel*>(model);
                    const glm::vec3 hand =
                        glm::vec3(biped->RightArmTransform(m) * glm::vec4(0.0f, 9.0f, 0.0f, 1.0f));
                    const glm::vec3 fwd{std::cos(yaw), 0.0f, std::sin(yaw)}; // aim direction
                    const glm::vec3 up{0.0f, 1.0f, 0.0f};
                    const glm::vec3 right{fwd.z, 0.0f, -fwd.x};
                    // The bow sits in the skeleton's sagittal (up x aim) plane —
                    // its flat face points sideways, so aiming at the player you
                    // see it edge-on (vanilla), the full profile from the side.
                    // The bow_standby sprite runs grip (bottom-left) -> tips
                    // (top-right), so the 45deg-rolled basis maps grip->tip to
                    // vertical and the curve depth to the aim direction.
                    constexpr float k = 0.70710678f; // cos/sin 45
                    constexpr float bs = 0.7f / 16.0f;
                    const glm::vec3 bx = (fwd + up) * k; // local +X (rolled, in the sagittal plane)
                    const glm::vec3 by = (up - fwd) * k; // local +Y
                    glm::mat4 bowM{1.0f};
                    bowM[0] = glm::vec4(bx * bs, 0.0f);
                    bowM[1] = glm::vec4(by * bs, 0.0f);
                    bowM[2] = glm::vec4(right * bs, 0.0f); // normal -> sideways (edge-on toward aim)
                    bowM[3] = glm::vec4(hand, 1.0f);
                    m_heldBow->Render(*m_entityModelShader, bowM);
                }
            }
            vox::Renderer::SetCullFace(true);
            m_chunkShader->Bind();
        }

        // M36: arrows in flight + stuck arrows. Same box-model shader/slot as the
        // mobs; one bind, then per-arrow orientation from its flight yaw/pitch
        // (verbatim RenderArrow transform).
        const auto& arrows = m_world->Entities().Arrows();
        if (m_arrowModel && m_arrowModel->Ready() && !arrows.empty()) {
            constexpr float kPi = 3.14159265358979323846f;
            const float a = static_cast<float>(alpha);
            m_entityModelShader->Bind();
            m_entityModelShader->SetMat4("u_viewProj", viewProj);
            m_entityModelShader->SetInt("u_skin", 1);
            m_entityModelShader->SetFloat3("u_sunDir", dayNight.lightDir);
            m_entityModelShader->SetFloat("u_sunLight", dayNight.sunLight);
            m_entityModelShader->SetFloat3("u_skyTint", dayNight.skyTint);
            m_entityModelShader->SetFloat("u_hurt", 0.0f);
            vox::Renderer::SetCullFace(false);
            for (const auto& arrow : arrows) {
                const glm::vec3 pos = glm::mix(arrow.prevPos, arrow.pos, a);
                const float yaw = glm::mix(arrow.prevYaw, arrow.yaw, a);
                const float pitch = glm::mix(arrow.prevPitch, arrow.pitch, a);
                const glm::ivec3 cell{static_cast<int>(std::floor(pos.x)),
                                      static_cast<int>(std::floor(pos.y)),
                                      static_cast<int>(std::floor(pos.z))};
                const uint8_t packed = m_world->PackedLightAt(cell);
                m_entityModelShader->SetFloat2(
                    "u_light", {static_cast<float>(vc::ChunkLight::Sky(packed)) / 15.0f,
                                static_cast<float>(vc::ChunkLight::Block(packed)) / 15.0f});
                // Vanilla RenderArrow: yaw-90 about Y, pitch about Z, a 45 deg
                // cross offset about X, scale 0.05625, then translate(-4,0,0).
                glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
                m = glm::rotate(m, yaw - kPi * 0.5f, {0.0f, 1.0f, 0.0f});
                m = glm::rotate(m, pitch, {0.0f, 0.0f, 1.0f});
                m = glm::rotate(m, kPi * 0.25f, {1.0f, 0.0f, 0.0f});
                m = glm::scale(m, glm::vec3(0.05625f));
                m = glm::translate(m, {-4.0f, 0.0f, 0.0f});
                m_arrowModel->Render(*m_entityModelShader, m);
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
        // M36/M37: feed the use charge — the bow draw shows pulling frames, a
        // food's eat progress drives the chewing shake (only one is non-zero).
        m_viewModel->SetUseProgress(std::max(BowDrawProgress(), EatProgress()));
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
