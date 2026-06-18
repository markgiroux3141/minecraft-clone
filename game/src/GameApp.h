#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "vox/audio/AudioEngine.h"
#include "vox/core/Application.h"
#include "vox/renderer/Camera.h"
#include "vox/renderer/Shader.h"
#include "vox/renderer/TextureArray.h"
#include "vox/renderer/UiRenderer.h"
#include "vox/renderer/VertexArray.h"

#include "InputState.h"
#include "entity/ChickenModel.h"
#include "entity/CowModel.h"
#include "entity/Entity.h"
#include "entity/HumanoidModel.h"
#include "entity/Mob.h"
#include "entity/MobModel.h"
#include "entity/PigModel.h"
#include "entity/SheepModel.h"
#include "item/Inventory.h"
#include "render/Particles.h"
#include "render/PlayerDoll.h"
#include "entity/Player.h"
#include "render/ViewModel.h"
#include "ui/BlockIcons.h"
#include "audio/Sounds.h"
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
    // inventory with the 2x2 grid, a crafting table's 3x3, or an open
    // furnace (free cursor, world keeps ticking, player physics run
    // input-less).
    enum class State : uint8_t { Title, Playing, Paused, Inventory, Crafting, Furnace, Dead };

    void SetPaused(bool paused);
    // M30: the player ran out of health — free the cursor, show the death
    // screen (world frozen). Respawn resets vitals and drops back to Playing.
    void EnterDeathScreen();
    void RespawnPlayer();
    bool ContainerOpen() const {
        return m_state == State::Inventory || m_state == State::Crafting ||
               m_state == State::Furnace;
    }
    void OpenContainer(State container); // Playing -> Inventory/Crafting/Furnace
    // Returns craft-grid contents + the carried stack to the inventory
    // (overflow thrown), re-arms the edit guards, back to Playing.
    // (Furnace slots stay in the furnace — its state lives in the world.)
    void CloseContainer();
    void RefreshWorldList();
    // Loads (or creates) saves/<name>; defaultSeed only matters for a brand
    // new save — an existing manifest's seed wins.
    void EnterWorld(const std::string& name, int defaultSeed);
    void CreateNewWorld();
    void ExitToTitle(); // persists player state, drops the world
    void PersistPlayerState();
    void HandleInput(double frameDt, int scroll); // scroll: wheel clicks this frame
    // M26: if the hand holds a bucket, do the fill/dump and return true so
    // RMB doesn't also try to place a block. False for any non-bucket item.
    bool TryUseBucket();
    // M34: if the hand holds shears and a shearable sheep is in reach (nearer
    // than any block target), shear it (wool drop + sound + shears wear) and
    // return true so RMB doesn't also place. False otherwise.
    bool TryShearSheep();
    // The block the camera eye sits in (air's def when none/no world).
    const vc::BlockDef& EyeLiquid() const;
    bool EyeInWater() const; // eye inside water (drives tint + fog)
    bool EyeInLava() const;  // eye inside lava (M26: orange tint + dense fog)
    void DrawTargetOutline();
    void DrawUi(); // HUD + menus; may change state (menu clicks)
    // Tosses an item entity forward from the eye (Q key, container throws).
    void ThrowItem(const vc::ItemStack& stack);

    // M22 audio. m_audio is declared first so it outlives m_sounds (which
    // holds a pointer to it) and the furnace-loop voices; OnShutdown also
    // tears it down explicitly before the world goes away.
    vox::AudioEngine m_audio;
    vc::GameSounds m_sounds;

    vox::PerspectiveCamera m_camera;
    Player m_player{m_camera};

    std::unique_ptr<vc::World> m_world;
    std::shared_ptr<vox::Shader> m_chunkShader;
    // Non-cube box geometry (torches): float model stream, same chunk.frag.
    std::shared_ptr<vox::Shader> m_modelShader;
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
    // M30: first-person fire overlay (16x512 animation strip; null = no overlay,
    // DrawUi falls back to a flickering orange tint).
    std::shared_ptr<vox::Texture2D> m_fireOverlay;
    double m_worldTime = 0.0; // ticks; one day/night cycle per kDayTicks

    // Entity cubes (falling blocks, block drops, crack overlay) and the
    // flat sprite quad for non-block item drops (sticks, tools).
    std::shared_ptr<vox::Shader> m_entityShader;
    std::shared_ptr<vox::VertexArray> m_entityCube;
    std::shared_ptr<vox::VertexArray> m_itemQuad;

    // M31: jointed box-model renderer (the M32 mob path + M33 player doll
    // build on this). For now a single debug Steve, toggled with G, walks a
    // small circle in front of the player to exercise the limb animation; no
    // gameplay, no collision, not persisted.
    std::shared_ptr<vox::Shader> m_entityModelShader;
    std::unique_ptr<vc::HumanoidModel> m_humanoid;
    // pos/prevPos/vel come from vc::Body, yaw/limbSwing*/age from vc::LivingAnim
    // (same bases as vc::Mob — no more copy-pasted walk-cycle fields).
    struct DebugMob : vc::Body, vc::LivingAnim {
        bool active = false;
        glm::vec3 center{0.0f}; // circle center (set when spawned)
        float angle = 0.0f;     // position around the circle, radians
    };
    DebugMob m_debugMob;
    void ToggleDebugMob();
    void TickDebugMob(double dt);

    // Mob box-models, one per MobType, behind the shared vc::IMobModel
    // interface so the render pass + sounds drive any mob uniformly (M34). The
    // World owns the mob ENTITIES; GameApp owns their MODELS. Each silently
    // skips drawing without its gitignored skin overlay (like the debug Steve).
    // Debug keys spawn one ahead of the player (B pig, C zombie, V cow, N sheep,
    // M chicken) so they're easy to test without waiting for natural spawns.
    std::array<std::unique_ptr<vc::IMobModel>, static_cast<size_t>(vc::MobType::Count)> m_mobModels;
    void SpawnMobAhead(vc::MobType type);
    // True when the sun is down (drives hostile spawn light gating).
    bool IsNight() const;

    // M20 game feel: block-break particles + the first-person hand.
    std::unique_ptr<vc::ParticleSystem> m_particles;
    std::unique_ptr<vc::ViewModel> m_viewModel;
    double m_chipAccum = 0.0; // dig hit-chip spawn pacing (one per tick)

    // M22 audio runtime state (furnace crackle voices, footstep cadence, dig
    // pacing) lives on vc::GameSounds — see m_sounds. GameApp only drives it.

    // Frustum-surviving chunks for the frame's multi-draws (reused scratch):
    // opaque front-to-back, then water back-to-front in the blended pass.
    std::vector<vox::MeshPool::DrawItem> m_drawItems;
    std::vector<vox::MeshPool::DrawItem> m_drawItemsTransparent;
    std::vector<vox::MeshPool::DrawItem> m_drawItemsModel; // torches etc.

    // 2D overlay (crosshair, hotbar; menus from M10 stage 2).
    std::unique_ptr<vox::UiRenderer> m_ui;
    // M29: baked 3D block-icon sheet for inventory/hotbar slots.
    std::unique_ptr<vc::BlockIcons> m_blockIcons;
    // M33: baked player-doll (body + worn armor) for the inventory screen.
    std::unique_ptr<vc::PlayerDoll> m_playerDoll;

    // Player items (M17): hotbar = inventory slots 0..8, keys 1..9 select;
    // m_carried rides the mouse while a container screen is open. The
    // craft grid (M19) is row-major: the 2x2 player grid uses the first
    // 4 cells, the table all 9; contents return to the bag on close.
    vc::Inventory m_inventory;
    vc::ItemStack m_carried;
    std::array<vc::ItemStack, 9> m_craftGrid;
    vc::GuiTextures m_guiTextures; // null members = placeholder look
    size_t m_hotbarSlot = 0;
    glm::ivec3 m_openFurnace{0}; // which furnace State::Furnace is showing
    int m_paletteScroll = 0;     // creative-palette scroll offset (rows); wheel-driven

    // Occlusion culling (cave culling) is on by default; O toggles it for
    // comparing drawn-chunk counts and spotting false culling.
    bool m_occlusionCulling = true;

    State m_state = State::Title;
    double m_deathAnim = 0.0; // seconds in the death screen (drives the keel-over tilt)
    std::filesystem::path m_savesRoot;
    std::vector<std::string> m_worlds; // directory names under m_savesRoot

    // Per-frame edge/repeat tracking for input (see InputState.h).
    vc::InputState m_input;

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
