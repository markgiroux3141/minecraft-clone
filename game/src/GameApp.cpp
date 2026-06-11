#include "GameApp.h"

#include <algorithm>
#include <format>
#include <random>

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
    m_hotbar = {vc::blocks::Stone, vc::blocks::Dirt, vc::blocks::Grass, vc::blocks::Glowstone};

    m_chunkShader = vox::Shader::FromFiles("shaders/chunk.vert", "shaders/chunk.frag");
    m_blockTextures = vox::Texture2DArray::FromFileStrip("textures/atlas.png", 16);

    m_ui = std::make_unique<vox::UiRenderer>();
    // Grid layout matches scripts/gen_font.py: ASCII 32..127, 16 cols x 6 rows.
    m_ui->SetFont(vox::Texture2D::FromFile("fonts/ascii.png"), 16, 6);

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
    m_world->SaveStore().SetPlayerState({m_player.Position(), m_player.Yaw(), m_player.Pitch(),
                                         m_player.GetMode() == Player::Mode::Fly});
}

void GameApp::OnTick(double dt) {
    if (m_world && m_state == State::Playing) {
        m_player.Tick(*m_world, dt);
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
        m_chunkShader->Bind();
        m_chunkShader->SetMat4("u_viewProj", m_camera.ViewProjection());
        m_chunkShader->SetInt("u_atlas", 0);
        m_blockTextures->Bind(0);

        const auto frustum = vox::Frustum::FromViewProjection(m_camera.ViewProjection());
        m_world->CollectVisibleChunks(m_camera.Position(), frustum, m_occlusionCulling,
                                      m_drawItems);
        m_world->Meshes().Draw(m_drawItems);
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
            GetWindow().SetTitle(std::format(
                "Voxcraft | {} fps | {} tps | ({:.0f}, {:.0f}, {:.0f}) | {} | hand: {} | chunks: "
                "{} loaded, {}/{} drawn, {} pending | {} jobs | {:.2f}M tris | pool {}/{} MB",
                m_frameCount, m_tickCount, pos.x, pos.y, pos.z,
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
