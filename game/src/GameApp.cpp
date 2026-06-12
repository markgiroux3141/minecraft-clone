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

#include "ui/Hud.h"
#include "ui/PauseMenu.h"
#include "ui/TitleScreen.h"
#include "world/Block.h"

namespace {

constexpr int kWorldSeed = 1337; // default for saves whose manifest lacks one
constexpr float kReachDistance = 5.0f;
constexpr double kEditRepeatDelay = 0.25; // held-button repeat, seconds

constexpr glm::vec3 kSpawnPos{8.5f, 48.0f, 8.5f};
constexpr float kSpawnYaw = 45.0f;
constexpr float kSpawnPitch = -15.0f;

// Day/night cycle: Minecraft's pacing (20 real minutes per day at 20 TPS).
// New worlds start shortly after sunrise; holding T fast-forwards.
constexpr double kDayTicks = 24000.0;
constexpr int64_t kNewWorldTime = 1000;
constexpr double kTimeFastForward = 200.0; // extra ticks per tick while T held

struct DayNight {
    glm::vec3 sunDir;
    float sunLight; // sky-light scale, moonlight floor at night
    glm::vec3 zenith;
    glm::vec3 horizon;
    glm::vec3 sunColor;
};

// t in [0,1): 0 sunrise, 0.25 noon, 0.5 sunset, 0.75 midnight.
DayNight ComputeDayNight(float t) {
    const float angle = t * glm::two_pi<float>();
    const float elevation = std::sin(angle); // -1..1
    DayNight dn;
    // East-west arc with a slight tilt so noon shadows aren't dead vertical.
    dn.sunDir = glm::normalize(glm::vec3(std::cos(angle), elevation, 0.25f));
    const float day = glm::smoothstep(-0.08f, 0.25f, elevation);
    dn.sunLight = 0.12f + 0.88f * day;
    dn.zenith = glm::mix(glm::vec3(0.015f, 0.025f, 0.07f), glm::vec3(0.22f, 0.51f, 0.92f), day);
    dn.horizon = glm::mix(glm::vec3(0.04f, 0.06f, 0.12f), glm::vec3(0.63f, 0.78f, 0.94f), day);
    // Warm band when the sun crosses the horizon, fading with elevation.
    const float sunset = std::max(0.0f, 1.0f - std::abs(elevation) * 5.0f);
    dn.horizon = glm::mix(dn.horizon, glm::vec3(0.93f, 0.49f, 0.26f), sunset * 0.65f);
    dn.sunColor = glm::mix(glm::vec3(1.0f, 0.45f, 0.20f), glm::vec3(1.0f, 0.95f, 0.85f), day);
    if (elevation < -0.15f) {
        dn.sunColor = glm::vec3(0.0f); // fully set — no disc (no moon yet)
    }
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
    m_hotbar = {vc::blocks::Stone, vc::blocks::Dirt, vc::blocks::Grass, vc::blocks::Glowstone,
                vc::blocks::Sand,  vc::blocks::Log,  vc::blocks::Leaves, vc::blocks::Water};

    m_chunkShader = vox::Shader::FromFiles("shaders/chunk.vert", "shaders/chunk.frag");
    m_blockTextures =
        vox::Texture2DArray::FromFileStrip(PreferMcAsset("textures/atlas.png"), 16);

    m_ui = std::make_unique<vox::UiRenderer>();
    // Grid layout matches scripts/gen_font.py: ASCII 32..127, 16 cols x 6 rows.
    m_ui->SetFont(vox::Texture2D::FromFile("fonts/ascii.png"), 16, 6);

    m_skyShader = vox::Shader::FromFiles("shaders/sky.vert", "shaders/sky.frag");
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

    m_state = State::Playing;
    m_target.reset();
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
}

void GameApp::OnTick(double dt) {
    if (m_world && m_state == State::Playing) {
        m_player.Tick(*m_world, dt);
        m_world->Tick(); // scheduled block updates (falling sand, water flow)
        m_worldTime += 1.0;
        if (vox::Input::IsKeyDown(vox::Key::T)) {
            m_worldTime += kTimeFastForward; // debug: watch the cycle quickly
        }
    }
    ++m_tickCount;
    ++m_totalTicks;
}

void GameApp::SetPaused(bool paused) {
    if (!m_world || (m_state == State::Paused) == paused) {
        return;
    }
    m_state = paused ? State::Paused : State::Playing;
    GetWindow().SetCursorCaptured(!paused);
    if (paused) {
        m_target.reset();
    } else {
        // Buttons held through the resume (e.g. the click that hit Resume)
        // must be re-pressed before they break or place anything.
        m_breakWasDown = true;
        m_placeWasDown = true;
        m_breakCooldown = kEditRepeatDelay;
        m_placeCooldown = kEditRepeatDelay;
    }
}

void GameApp::HandleInput(double frameDt) {
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

    // 1..N select the hotbar block.
    for (size_t i = 0; i < m_hotbar.size(); ++i) {
        if (vox::Input::IsKeyDown(static_cast<vox::Key>(static_cast<int>(vox::Key::Num1) + i))) {
            m_hotbarSlot = i;
        }
    }

    // Aim from the eye; break/place act on press, then repeat while held.
    m_target = m_world->RaycastBlocks(m_camera.Position(), m_camera.Forward(), kReachDistance);
    m_breakCooldown = std::max(0.0, m_breakCooldown - frameDt);
    m_placeCooldown = std::max(0.0, m_placeCooldown - frameDt);

    const bool breakDown = vox::Input::IsMouseButtonDown(vox::MouseButton::Left);
    if (breakDown && m_target && (!m_breakWasDown || m_breakCooldown == 0.0)) {
        m_world->SetBlock(m_target->block, vc::blocks::Air);
        m_breakCooldown = kEditRepeatDelay;
    }
    m_breakWasDown = breakDown;

    const bool placeDown = vox::Input::IsMouseButtonDown(vox::MouseButton::Right);
    if (placeDown && m_target && (!m_placeWasDown || m_placeCooldown == 0.0)) {
        const glm::ivec3 cell = m_target->block + m_target->normal;
        if (!m_world->IsSolid(cell.x, cell.y, cell.z) && !m_player.Intersects(cell)) {
            m_world->SetBlock(cell, m_hotbar[m_hotbarSlot]);
            m_placeCooldown = kEditRepeatDelay;
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

    const glm::vec2 mouse = vox::Input::MousePosition();

    m_ui->Begin(GetWindow().Width(), GetWindow().Height(), m_blockTextures.get());
    if (EyeInWater()) {
        // Underwater tint on top of the shader fog.
        m_ui->DrawRect({0.0f, 0.0f}, screen, {0.09f, 0.27f, 0.55f, 0.35f});
    }
    if (m_state == State::Title) {
        const auto action = vc::TitleScreen::Draw(*m_ui, screen, mouse, clicked, m_worlds);
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
        vc::Hud::Draw(*m_ui, screen, m_hotbar, m_hotbarSlot);
        if (m_state == State::Paused) {
            const auto action = vc::PauseMenu::Draw(*m_ui, screen, mouse, clicked);
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
        SetPaused(m_state == State::Playing); // no-op on the title screen
    }
    m_escapeWasDown = escapeDown;

    if (m_world) {
        const bool playing = m_state == State::Playing;
        m_player.OnRender(alpha, playing);
        if (playing) {
            HandleInput(frameDt);
        }
        m_world->Update(m_camera.Position());
    }

    vox::Renderer::Clear();

    if (m_world) {
        const double dayFraction =
            std::fmod((m_worldTime + (m_state == State::Playing ? alpha : 0.0)) / kDayTicks, 1.0);
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
        vox::Renderer::SetDepthWrite(false);
        vox::Renderer::DrawIndexed(*m_skyQuad);
        vox::Renderer::SetDepthWrite(true);

        m_chunkShader->Bind();
        m_chunkShader->SetMat4("u_viewProj", viewProj);
        m_chunkShader->SetInt("u_atlas", 0);
        m_chunkShader->SetFloat3("u_sunDir", dayNight.sunDir);
        m_chunkShader->SetFloat("u_sunLight", dayNight.sunLight);
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
                                      m_drawItems, m_drawItemsTransparent);
        m_world->Meshes().Draw(m_drawItems);

        // Falling-block entities: opaque, so before the water pass (which
        // expects the chunk shader bound again afterwards).
        const auto& fallingBlocks = m_world->FallingBlocks();
        if (!fallingBlocks.empty()) {
            m_entityShader->Bind();
            m_entityShader->SetMat4("u_viewProj", viewProj);
            m_entityShader->SetInt("u_atlas", 0);
            m_entityShader->SetFloat3("u_sunDir", dayNight.sunDir);
            m_entityShader->SetFloat("u_sunLight", dayNight.sunLight);
            vox::Renderer::SetCullFace(false); // a handful of cubes; skip winding care
            for (const auto& falling : fallingBlocks) {
                if (!m_world->FallingBlockVisible(falling)) {
                    continue; // mesh handover in progress (no double-draw/gap)
                }
                const float y = glm::mix(falling.prevY, falling.y, static_cast<float>(alpha));
                const auto& def = vc::BlockRegistry::Get().Def(falling.id);
                for (int face = 0; face < 6; ++face) {
                    m_entityShader->SetFloat(std::format("u_faceLayers[{}]", face),
                                             static_cast<float>(def.faceTiles[face]));
                }
                // Sample the cell AND the cell above and take the max:
                // once the block is placed, its own (opaque) cell reads
                // light 0 and the lingering cube would flash black.
                const glm::ivec3 cell{falling.x, static_cast<int>(std::floor(y + 0.5f)),
                                      falling.z};
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
                m_entityShader->SetFloat3("u_origin", {static_cast<float>(falling.x), y,
                                                       static_cast<float>(falling.z)});
                vox::Renderer::DrawIndexed(*m_entityCube);
            }
            vox::Renderer::SetCullFace(true);
            m_chunkShader->Bind();
        }

        if (!m_drawItemsTransparent.empty()) {
            // Water: blended, depth-tested but not depth-written, double-
            // sided so the surface shows from below too.
            vox::Renderer::SetBlend(true);
            vox::Renderer::SetDepthWrite(false);
            vox::Renderer::SetCullFace(false);
            m_world->Meshes().Draw(m_drawItemsTransparent);
            vox::Renderer::SetBlend(false);
            vox::Renderer::SetDepthWrite(true);
            vox::Renderer::SetCullFace(true);
        }
        m_chunksDrawn = m_drawItems.size();

        DrawTargetOutline();
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
                vc::BlockRegistry::Get().Def(m_hotbar[m_hotbarSlot]).name,
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
    GAME_INFO("Voxcraft shutting down after {} ticks", m_totalTicks);
}
