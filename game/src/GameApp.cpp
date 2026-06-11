#include "GameApp.h"

#include <format>

#include <glm/gtc/matrix_transform.hpp>

#include "vox/core/Log.h"
#include "vox/platform/Input.h"
#include "vox/renderer/Frustum.h"
#include "vox/renderer/Renderer.h"

#include "world/Block.h"

namespace {

constexpr int kWorldSeed = 1337;
constexpr float kReachDistance = 5.0f;
constexpr double kEditRepeatDelay = 0.25; // held-button repeat, seconds

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
    GAME_INFO("Voxcraft starting up (seed {})", kWorldSeed);
    vox::Renderer::SetClearColor(0.45f, 0.70f, 1.00f); // placeholder sky

    vc::blocks::RegisterDefaults();
    m_hotbar = {vc::blocks::Stone, vc::blocks::Dirt, vc::blocks::Grass, vc::blocks::Glowstone};

    m_chunkShader = vox::Shader::FromFiles("shaders/chunk.vert", "shaders/chunk.frag");
    m_blockTextures = vox::Texture2DArray::FromFileStrip("textures/atlas.png", 16);

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

    m_world = std::make_unique<vc::World>(kWorldSeed);

    m_camera.SetPerspective(70.0f, 0.1f, 1000.0f);
    m_camera.SetViewportSize(GetWindow().Width(), GetWindow().Height());
    m_player.Teleport({8.5f, 48.0f, 8.5f});
    m_player.SetLook(45.0f, -15.0f);
    GetWindow().SetCursorCaptured(true);
}

void GameApp::OnTick(double dt) {
    m_player.Tick(*m_world, dt);
    ++m_tickCount;
    ++m_totalTicks;
}

void GameApp::HandleInput(double frameDt) {
    // F toggles walk/fly.
    const bool modeKey = vox::Input::IsKeyDown(vox::Key::F);
    if (modeKey && !m_modeKeyWasDown) {
        m_player.ToggleMode();
    }
    m_modeKeyWasDown = modeKey;

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

void GameApp::OnRender(double alpha, double frameDt) {
    if (vox::Input::IsKeyDown(vox::Key::Escape)) {
        Close();
    }

    m_player.OnRender(alpha);
    HandleInput(frameDt);
    m_world->Update(m_camera.Position());

    vox::Renderer::Clear();

    m_chunkShader->Bind();
    m_chunkShader->SetMat4("u_viewProj", m_camera.ViewProjection());
    m_chunkShader->SetInt("u_atlas", 0);
    m_blockTextures->Bind(0);

    const auto frustum = vox::Frustum::FromViewProjection(m_camera.ViewProjection());
    m_chunksDrawn = 0;
    m_chunksWithMesh = 0;
    m_trianglesLoaded = 0;
    m_world->ForEachRenderableChunk(
        [&](const glm::ivec3& coord, const vox::VertexArray& mesh, uint32_t indexCount) {
            ++m_chunksWithMesh;
            m_trianglesLoaded += indexCount / 3;
            const glm::vec3 min = glm::vec3(coord * vc::Chunk::kSize);
            const glm::vec3 max = min + glm::vec3(static_cast<float>(vc::Chunk::kSize));
            if (!frustum.IntersectsAABB(min, max)) {
                return;
            }
            m_chunkShader->SetMat4("u_model", glm::translate(glm::mat4(1.0f), min));
            vox::Renderer::DrawIndexed(mesh, indexCount);
            ++m_chunksDrawn;
        });

    DrawTargetOutline();

    ++m_frameCount;
    m_statsTimer += frameDt;
    if (m_statsTimer >= 1.0) {
        const auto pos = m_camera.Position();
        GetWindow().SetTitle(std::format(
            "Voxcraft | {} fps | {} tps | ({:.0f}, {:.0f}, {:.0f}) | {} | hand: {} | chunks: {} "
            "loaded, {}/{} drawn, {} pending | {} jobs | {:.2f}M tris",
            m_frameCount, m_tickCount, pos.x, pos.y, pos.z,
            m_player.GetMode() == Player::Mode::Fly ? "fly" : "walk",
            vc::BlockRegistry::Get().Def(m_hotbar[m_hotbarSlot]).name,
            m_world->LoadedChunkCount(), m_chunksDrawn, m_chunksWithMesh,
            m_world->PendingMeshCount(), m_world->JobsInFlight(),
            static_cast<double>(m_trianglesLoaded) / 1e6));
        m_statsTimer -= 1.0;
        m_frameCount = 0;
        m_tickCount = 0;
    }
}

void GameApp::OnResize(uint32_t width, uint32_t height) {
    m_camera.SetViewportSize(width, height);
}

void GameApp::OnShutdown() {
    GAME_INFO("Voxcraft shutting down after {} ticks", m_totalTicks);
}
