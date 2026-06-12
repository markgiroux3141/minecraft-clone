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

#include "Inventory.h"
#include "Particles.h"
#include "Player.h"
#include "ViewModel.h"
#include "ui/Widgets.h"
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
    // the pause menu (free cursor, sim frozen), or a container screen —
    // inventory with the 2x2 grid, or a crafting table's 3x3 (free
    // cursor, world keeps ticking, player physics run input-less).
    enum class State : uint8_t { Title, Playing, Paused, Inventory, Crafting };

    void SetPaused(bool paused);
    bool ContainerOpen() const {
        return m_state == State::Inventory || m_state == State::Crafting;
    }
    void OpenContainer(bool table); // Playing -> Inventory/Crafting
    // Returns craft-grid contents + the carried stack to the inventory
    // (overflow thrown), re-arms the edit guards, back to Playing.
    void CloseContainer();
    void RefreshWorldList();
    // Loads (or creates) saves/<name>; defaultSeed only matters for a brand
    // new save — an existing manifest's seed wins.
    void EnterWorld(const std::string& name, int defaultSeed);
    void CreateNewWorld();
    void ExitToTitle(); // persists player state, drops the world
    void PersistPlayerState();
    void HandleInput(double frameDt, int scroll); // scroll: wheel clicks this frame
    bool EyeInWater() const;
    void DrawTargetOutline();
    void DrawUi(); // HUD + menus; may change state (menu clicks)
    // Tosses an item entity forward from the eye (Q key, container throws).
    void ThrowItem(const vc::ItemStack& stack);

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
    // Textured sun/moon (assets/mc only; null = procedural sky.frag disc).
    std::shared_ptr<vox::Shader> m_celestialShader;
    std::shared_ptr<vox::Texture2D> m_sunTexture;
    std::shared_ptr<vox::Texture2D> m_moonTexture;
    double m_worldTime = 0.0; // ticks; one day/night cycle per kDayTicks

    // Entity cubes (falling blocks, block drops, crack overlay) and the
    // flat sprite quad for non-block item drops (sticks, tools).
    std::shared_ptr<vox::Shader> m_entityShader;
    std::shared_ptr<vox::VertexArray> m_entityCube;
    std::shared_ptr<vox::VertexArray> m_itemQuad;

    // M20 game feel: block-break particles + the first-person hand.
    std::unique_ptr<vc::ParticleSystem> m_particles;
    std::unique_ptr<vc::ViewModel> m_viewModel;
    double m_chipAccum = 0.0; // dig hit-chip spawn pacing (one per tick)

    // Frustum-surviving chunks for the frame's multi-draws (reused scratch):
    // opaque front-to-back, then water back-to-front in the blended pass.
    std::vector<vox::MeshPool::DrawItem> m_drawItems;
    std::vector<vox::MeshPool::DrawItem> m_drawItemsTransparent;

    // 2D overlay (crosshair, hotbar; menus from M10 stage 2).
    std::unique_ptr<vox::UiRenderer> m_ui;

    // Player items (M17): hotbar = inventory slots 0..8, keys 1..9 select;
    // m_carried rides the mouse while a container screen is open. The
    // craft grid (M19) is row-major: the 2x2 player grid uses the first
    // 4 cells, the table all 9; contents return to the bag on close.
    vc::Inventory m_inventory;
    vc::ItemStack m_carried;
    std::array<vc::ItemStack, 9> m_craftGrid;
    vc::GuiTextures m_guiTextures; // null members = placeholder look
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
    bool m_inventoryKeyWasDown = false;
    bool m_clickWasDown = false; // menu clicks (break/place track separately)
    bool m_rightClickWasDown = false;
    bool m_breakWasDown = false;
    bool m_placeWasDown = false;
    bool m_dropKeyWasDown = false;
    double m_breakCooldown = 0.0; // fly repeat AND the walk-dig hit delay
    double m_placeCooldown = 0.0;
    double m_dropCooldown = 0.0;

    // Hold-to-break (M18, walk mode): the block being dug and its damage
    // 0..1 (vanilla curBlockDamageMP). Resets when the target changes or
    // the button lifts; crack stage = int(progress * 10) - 1.
    std::optional<glm::ivec3> m_digCell;
    float m_digProgress = 0.0f;

    // Rolling counters for the once-per-second title-bar stats.
    double m_statsTimer = 0.0;
    uint32_t m_frameCount = 0;
    uint32_t m_tickCount = 0;
    uint64_t m_totalTicks = 0;
    size_t m_chunksDrawn = 0;
    size_t m_chunksWithMesh = 0;
    uint64_t m_trianglesLoaded = 0;
};
