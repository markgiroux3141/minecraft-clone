#pragma once

#include <memory>

#include "vox/core/Application.h"
#include "vox/renderer/Camera.h"
#include "vox/renderer/Shader.h"
#include "vox/renderer/TextureArray.h"

#include "FlyCamera.h"
#include "world/World.h"

class GameApp : public vox::Application {
public:
    GameApp();

protected:
    void OnInit() override;
    void OnTick(double dt) override;
    void OnRender(double alpha, double frameDt) override;
    void OnResize(uint32_t width, uint32_t height) override;
    void OnShutdown() override;

private:
    vox::PerspectiveCamera m_camera;
    FlyCamera m_flyCamera{m_camera};

    std::unique_ptr<vc::World> m_world;
    std::shared_ptr<vox::Shader> m_chunkShader;
    std::shared_ptr<vox::Texture2DArray> m_blockTextures;

    // Rolling counters for the once-per-second title-bar stats.
    double m_statsTimer = 0.0;
    uint32_t m_frameCount = 0;
    uint32_t m_tickCount = 0;
    uint64_t m_totalTicks = 0;
    size_t m_chunksDrawn = 0;
    size_t m_chunksWithMesh = 0;
};
