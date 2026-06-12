#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "vox/core/Application.h"
#include "vox/renderer/Camera.h"
#include "vox/renderer/Shader.h"
#include "vox/renderer/TextureArray.h"
#include "vox/renderer/UiRenderer.h"
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
    // Title/world-select (no world, free cursor), captured-cursor gameplay,
    // or the pause menu over a live world (free cursor).
    enum class State : uint8_t { Title, Playing, Paused };

    void SetPaused(bool paused);
    void RefreshWorldList();
    // Loads (or creates) saves/<name>; defaultSeed only matters for a brand
    // new save — an existing manifest's seed wins.
    void EnterWorld(const std::string& name, int defaultSeed);
    void CreateNewWorld();
    void ExitToTitle(); // persists player state, drops the world
    void PersistPlayerState();
    void HandleInput(double frameDt);
    bool EyeInWater() const;
    void DrawTargetOutline();
    void DrawUi(); // HUD + menus; may change state (menu clicks)

    vox::PerspectiveCamera m_camera;
    Player m_player{m_camera};

    std::unique_ptr<vc::World> m_world;
    std::shared_ptr<vox::Shader> m_chunkShader;
    std::shared_ptr<vox::Texture2DArray> m_blockTextures;

    // Targeted-block wireframe.
    std::shared_ptr<vox::Shader> m_outlineShader;
    std::shared_ptr<vox::VertexArray> m_outlineCube;
    std::optional<vc::World::RaycastHit> m_target;

    // Sky dome (fullscreen gradient + sun disc) and the day/night clock.
    std::shared_ptr<vox::Shader> m_skyShader;
    std::shared_ptr<vox::VertexArray> m_skyQuad;
    double m_worldTime = 0.0; // ticks; one day/night cycle per kDayTicks

    // Falling-block entities: one textured unit cube, drawn per entity.
    std::shared_ptr<vox::Shader> m_entityShader;
    std::shared_ptr<vox::VertexArray> m_entityCube;

    // Frustum-surviving chunks for the frame's multi-draws (reused scratch):
    // opaque front-to-back, then water back-to-front in the blended pass.
    std::vector<vox::MeshPool::DrawItem> m_drawItems;
    std::vector<vox::MeshPool::DrawItem> m_drawItemsTransparent;

    // 2D overlay (crosshair, hotbar; menus from M10 stage 2).
    std::unique_ptr<vox::UiRenderer> m_ui;

    // Hotbar: keys 1..N select into this list (filled after block registration).
    std::array<vc::BlockId, 8> m_hotbar{};
    size_t m_hotbarSlot = 0;

    // Occlusion culling (cave culling) is on by default; O toggles it for
    // comparing drawn-chunk counts and spotting false culling.
    bool m_occlusionCulling = true;

    State m_state = State::Title;
    std::filesystem::path m_savesRoot;
    std::vector<std::string> m_worlds; // directory names under m_savesRoot

    // Edge/repeat tracking for per-frame input.
    bool m_modeKeyWasDown = false;
    bool m_occlusionKeyWasDown = false;
    bool m_escapeWasDown = false;
    bool m_clickWasDown = false; // menu clicks (break/place track separately)
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
