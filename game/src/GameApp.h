#pragma once

#include <array>
#include <memory>
#include <optional>

#include "vox/core/Application.h"
#include "vox/renderer/Camera.h"
#include "vox/renderer/Shader.h"
#include "vox/renderer/TextureArray.h"
#include "vox/renderer/VertexArray.h"

#include "Player.h"
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
    void HandleInput(double frameDt);
    void DrawTargetOutline();

    vox::PerspectiveCamera m_camera;
    Player m_player{m_camera};

    std::unique_ptr<vc::World> m_world;
    std::shared_ptr<vox::Shader> m_chunkShader;
    std::shared_ptr<vox::Texture2DArray> m_blockTextures;

    // Targeted-block wireframe.
    std::shared_ptr<vox::Shader> m_outlineShader;
    std::shared_ptr<vox::VertexArray> m_outlineCube;
    std::optional<vc::World::RaycastHit> m_target;

    // Hotbar: keys 1..N select into this list (filled after block registration).
    std::array<vc::BlockId, 3> m_hotbar{};
    size_t m_hotbarSlot = 0;

    // Edge/repeat tracking for per-frame input.
    bool m_modeKeyWasDown = false;
    bool m_breakWasDown = false;
    bool m_placeWasDown = false;
    double m_breakCooldown = 0.0;
    double m_placeCooldown = 0.0;

    // Rolling counters for the once-per-second title-bar stats.
    double m_statsTimer = 0.0;
    uint32_t m_frameCount = 0;
    uint32_t m_tickCount = 0;
    uint64_t m_totalTicks = 0;
    size_t m_chunksDrawn = 0;
    size_t m_chunksWithMesh = 0;
    uint64_t m_trianglesLoaded = 0;
};
