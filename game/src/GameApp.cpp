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

    m_chunkShader = vox::Shader::FromFiles("shaders/chunk.vert", "shaders/chunk.frag");
    m_blockTextures = vox::Texture2DArray::FromFileStrip("textures/atlas.png", 16);

    m_world = std::make_unique<vc::World>(kWorldSeed);

    m_camera.SetPerspective(70.0f, 0.1f, 1000.0f);
    m_camera.SetViewportSize(GetWindow().Width(), GetWindow().Height());
    m_camera.SetPosition({8.0f, 56.0f, 8.0f});
    m_camera.LookAt({60.0f, 20.0f, 60.0f});
}

void GameApp::OnTick(double /*dt*/) {
    ++m_tickCount;
    ++m_totalTicks;
}

void GameApp::OnRender(double /*alpha*/, double frameDt) {
    if (vox::Input::IsKeyDown(vox::Key::Escape)) {
        Close();
    }

    m_flyCamera.OnUpdate(frameDt);
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

    ++m_frameCount;
    m_statsTimer += frameDt;
    if (m_statsTimer >= 1.0) {
        const auto pos = m_camera.Position();
        GetWindow().SetTitle(std::format(
            "Voxcraft | {} fps | {} tps | ({:.0f}, {:.0f}, {:.0f}) | chunks: {} loaded, "
            "{}/{} drawn, {} pending | {} jobs | {:.2f}M tris",
            m_frameCount, m_tickCount, pos.x, pos.y, pos.z, m_world->LoadedChunkCount(),
            m_chunksDrawn, m_chunksWithMesh, m_world->PendingMeshCount(),
            m_world->JobsInFlight(), static_cast<double>(m_trianglesLoaded) / 1e6));
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
