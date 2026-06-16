#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "vox/core/ThreadPool.h"
#include "vox/renderer/Frustum.h"
#include "vox/renderer/MeshPool.h"

#include "entity/Entity.h" // Body base for ItemEntity
#include "entity/Mob.h"
#include "world/Chunk.h"
#include "world/ChunkMesher.h"
#include "world/Furnace.h"
#include "world/Light.h"
#include "world/LightEngine.h"
#include "world/TerrainGen.h"
#include "world/WorldSave.h"

namespace vc {

// Per-chunk record. Block and light data are copy-on-write: the pointers
// are replaced (never mutated), so worker-thread snapshots stay valid
// without locks. Mesh freshness is tracked with versions — in-flight
// results are matched against dataVersion and dropped when stale.
struct ChunkEntry {
    std::shared_ptr<const Chunk> blocks;     // null while the gen job is in flight
    std::shared_ptr<const ChunkLight> light; // null until the column is first lit
    uint32_t dataVersion = 0;   // mesh-input version: bumped by edits AND light changes
    uint32_t meshedVersion = 0; // dataVersion the uploaded mesh was built from
    uint32_t meshingVersion = 0; // dataVersion the in-flight mesh job targets (0 = none)
    // Allocations in World's MeshPool; invalid until uploaded, or if empty.
    // meshT is the chunk's liquid faces, drawn in the blended pass.
    vox::MeshPool::MeshHandle mesh = vox::MeshPool::kInvalidMesh;
    vox::MeshPool::MeshHandle meshT = vox::MeshPool::kInvalidMesh;
    // Non-cube box geometry (torches), in the separate float model pool.
    vox::MeshPool::MeshHandle meshM = vox::MeshPool::kInvalidMesh;
    uint32_t indexCount = 0;
    uint32_t indexCountT = 0;
    uint32_t indexCountM = 0;
    VisibilityBits visibility = 0; // face connectivity, valid once meshedVersion != 0
    bool edited = false; // diverges from the save store — persist before unload
};

// One LOD column: two stacked half-res chunks (world height 4 chunks /
// LOD scale 2) covering 32x32 world blocks. Generated once from real
// terrain (no edits, no relighting), so plain flags replace the version
// counters of full-detail chunks. Stacks world-height/scale chunks (4 in
// the M25 128-tall world).
inline constexpr int kLodHeightChunks = kWorldHeightChunks / 2;
// Aggregate-init helper: an array of N mesh handles all set to kInvalidMesh
// (0xFFFFFFFF, not 0 — value-init would alias real slot 0).
template <size_t N>
constexpr std::array<vox::MeshPool::MeshHandle, N> InvalidMeshes() {
    std::array<vox::MeshPool::MeshHandle, N> a{};
    a.fill(vox::MeshPool::kInvalidMesh);
    return a;
}
struct LodColumnEntry {
    std::array<std::shared_ptr<const Chunk>, kLodHeightChunks> cells; // null until gen lands
    std::array<vox::MeshPool::MeshHandle, kLodHeightChunks> mesh = InvalidMeshes<kLodHeightChunks>();
    std::array<vox::MeshPool::MeshHandle, kLodHeightChunks> meshT =
        InvalidMeshes<kLodHeightChunks>();
    std::array<uint32_t, kLodHeightChunks> indexCount{};
    std::array<uint32_t, kLodHeightChunks> indexCountT{};
    bool meshInFlight = false;
    bool meshed = false;
};

// Per-column light bookkeeping (light is computed column-at-a-time because
// sky light depends on the whole column above). Same versioning pattern as
// chunk meshes; the resulting ChunkLight sections live on the ChunkEntries.
struct ColumnEntry {
    uint32_t dirtySeq = 1;    // bumped when blocks change within light range
    uint32_t litSeq = 0;      // dirtySeq the stored light was computed at
    uint32_t lightingSeq = 0; // dirtySeq the in-flight light job targets (0 = none)
};

// Owns all loaded chunks and streams them around the camera. Workers
// produce CPU-side data (generated chunks, column light, chunk meshes) and
// push it to completion queues; Update() drains those queues, mutates the
// maps, and uploads meshes to the GPU — the maps and GL are
// main-thread-only.
//
// Radius layering (each stage needs a 3x3 neighborhood of the previous):
// generate at kViewRadius+2, light at kViewRadius+1, mesh at kViewRadius.
class World {
public:
    static constexpr int kHeightChunks = kWorldHeightChunks; // world height: 64 blocks
    static constexpr int kHeightBlocks = kWorldHeightBlocks;
    static constexpr int kViewRadius = 12; // horizontal full-detail mesh radius, in chunks

    // Half-res LOD shell past the detail radius: one LOD chunk is 16^3
    // cells of 2^3 blocks (32^3 world blocks, world height = 2 LOD
    // chunks), downsampled from real generator output and drawn at 2x
    // scale out to kLodRadius regular columns. A LOD column is drawn when
    // its far corner is outside the detail radius, so depending on grid
    // parity it may overlap the outermost detail ring by one column — the
    // solid-biased downsample makes that a <= 1 block bulge 200+ blocks
    // away, which beats a one-column gap on the other side.
    static constexpr int kLodScale = 2;
    static_assert(kLodHeightChunks == kHeightChunks / kLodScale);
    static constexpr int kLodRadius = 32; // outer draw bound, near-corner distance
    // Data ring: one LOD column (2 regular) past the draw bounds on each
    // side, so boundary LOD meshes have their 3x3 neighborhood.
    static constexpr int kLodDataOuter = kLodRadius + 2 * kLodScale;
    static constexpr int kLodDataInner = kViewRadius - 2 * kLodScale;

    // defaultSeed only applies to a brand-new save; an existing save's
    // manifest seed wins so its untouched chunks regenerate identically.
    World(int defaultSeed, std::filesystem::path saveDir);
    ~World(); // saves all edited chunks and force-flushes the store

    void Update(const glm::vec3& cameraPos);

    // Fixed-rate simulation step (20 TPS, from GameApp::OnTick): runs the
    // scheduled block updates that drive falling sand (and water flow).
    // SetBlock schedules the edited cell and its neighbors automatically,
    // so cascades sustain themselves until everything settles.
    void Tick();
    void ScheduleBlockUpdate(const glm::ivec3& worldPos, int delayTicks);

    // A detached gravity block in flight: an unsupported sand block turns
    // into one of these (smooth, accelerating fall), then turns back into
    // a block on landing. Tick-simulated; the renderer interpolates
    // prevY/y with the frame alpha, like the player.
    //
    // Remeshing is asynchronous, so the entity and the chunk mesh must
    // hand over without overlap: at spawn the entity stays HIDDEN until
    // the mesh stops showing the removed block (no z-fighting), and at
    // landing it keeps drawing until the mesh shows the placed block (no
    // one-frame gap). syncCell/syncVersion track the chunk edit to wait
    // for; FallingBlockVisible answers for the renderer.
    struct FallingBlock {
        int x, z; // stays on its column
        float y, prevY;
        float velocity;
        BlockId id;
        glm::ivec3 syncCell;
        uint32_t syncVersion;
        bool landed; // physics done; lingers until the mesh catches up
    };
    const std::vector<FallingBlock>& FallingBlocks() const { return m_fallingBlocks; }
    bool FallingBlockVisible(const FallingBlock& falling) const;

    // A dropped item in the world (M18): a mini block cube with vanilla
    // EntityItem physics — gravity 0.04 b/tick^2, drag x0.98/tick (ground
    // friction x0.6 horizontally), merges with same-id neighbors within
    // 0.5 blocks, despawns at age 6000 (5 min). Tick-simulated and
    // interpolated like FallingBlock. NOT persisted — drops vanish on
    // save/quit (they'd despawn within minutes anyway).
    // Inherits Body (pos/prevPos/vel, bottom-center of the cube, render-
    // interpolated) — shared with Mob.
    struct ItemEntity : Body {
        uint16_t id; // unified item id (block or registry item, see Item.h)
        int count;
        int damage = 0;       // tool wear rides along (M19)
        int age = 0;
        int pickupDelay = 10; // ticks (vanilla default; throws use 40)
        float phase = 0.0f;   // render bob/spin offset (vanilla hoverStart)
    };
    const std::vector<ItemEntity>& ItemEntities() const { return m_itemEntities; }

    // Block-drop spawn, vanilla Block.spawnAsEntity: jittered to
    // cell + 0.25..0.75 with a small random scatter velocity. No-op when
    // id is air or count <= 0. damage rides along (furnace spills can
    // hold worn tools).
    void SpawnBlockDrop(const glm::ivec3& cell, uint16_t id, int count, int damage = 0);
    // Explicit spawn (player throws): pickupDelay 40 like vanilla's drops.
    void SpawnItem(const glm::vec3& pos, const glm::vec3& vel, uint16_t id, int count,
                   int pickupDelay, int damage = 0);
    // Items overlapping box min/max (caller grows the player AABB by
    // vanilla's 1.0/0.5/1.0 pickup reach) whose delay has expired.
    // take(id, count, damage) returns how many fit; fully-taken items are
    // removed.
    template <typename Fn> void PickupItems(const glm::vec3& boxMin, const glm::vec3& boxMax, Fn&& take) {
        for (size_t i = 0; i < m_itemEntities.size();) {
            ItemEntity& item = m_itemEntities[i];
            const glm::vec3 c = item.pos; // bottom-center, half extent 0.125
            if (item.pickupDelay > 0 || c.x + 0.125f < boxMin.x || c.x - 0.125f > boxMax.x ||
                c.z + 0.125f < boxMin.z || c.z - 0.125f > boxMax.z || c.y + 0.25f < boxMin.y ||
                c.y > boxMax.y) {
                ++i;
                continue;
            }
            item.count -= take(item.id, item.count, item.damage);
            if (item.count <= 0) {
                m_itemEntities.erase(m_itemEntities.begin() + static_cast<ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
    }

    // --- M32 mobs ----------------------------------------------------------
    // World stays Player- and audio-agnostic, so mob AI gets the player state
    // it needs (feet + AABB + whether it's night for hostile spawns) and two
    // callbacks: damagePlayer (a hostile's melee, with the source position for
    // knockback) and pushPlayer (soft body collision shoving the player aside).
    struct MobTickCtx {
        glm::vec3 playerFeet{0.0f};
        float playerHalfWidth = 0.3f;
        float playerHeight = 1.8f;
        bool isNight = false;
        std::function<void(float dmg, const glm::vec3& fromPos)> damagePlayer;
        std::function<void(float dx, float dz)> pushPlayer;
    };
    // One 20-TPS step of every mob: AI -> ground physics (gravity + axis-
    // separated AABB collision with 1-block auto-step) -> walk-cycle anim ->
    // combat -> soft entity push -> fall damage/death; then periodic natural
    // spawning and far-mob despawn. Call from GameApp::OnTick.
    void TickMobs(const MobTickCtx& ctx);
    // Spawn a mob standing at feetPos (debug spawn key; natural spawns use it
    // too). Clamped to nothing — caller picks a valid spot.
    void SpawnMob(MobType type, const glm::vec3& feetPos);
    const std::vector<Mob>& Mobs() const { return m_mobs; }
    // Ray vs mob AABBs (player melee). Returns the nearest hit mob's index and
    // its distance in outDist; nothing past maxDist.
    std::optional<size_t> RaycastMob(const glm::vec3& origin, const glm::vec3& dir,
                                     float maxDist, float& outDist) const;
    // Damage a mob (player melee): health, red-flash, knockback away from
    // fromPos, hurt/death sound event; drops loot and removes it at <= 0.
    void DamageMob(size_t index, float amount, const glm::vec3& fromPos);
    // Mob sounds the sim wants played this tick (hurt/death). GameApp drains
    // and clears it after Tick, keeping World audio-free (like ForEachLitFurnace).
    std::vector<MobSound>& MobSoundEvents() { return m_mobSounds; }

    // Per-furnace block-entity state (M21), keyed by world position.
    // Created empty on first access (RMB-open does it); ticked by Tick()
    // while the chunk is loaded; spilled as drops and erased when the
    // furnace block is replaced (see SetBlock). Persisted via the save
    // store's furnaces.dat sidecar.
    FurnaceState& FurnaceAt(const glm::ivec3& worldPos) { return m_furnaces[worldPos]; }

    // M22: invoke fn(worldPos) for each tracked furnace whose block is
    // currently lit (burning). Lets the audio layer keep one crackle loop per
    // lit furnace without World ever knowing about audio. Skips furnaces in
    // unloaded chunks (GetBlock returns air there), so loops stop on unload.
    template <typename Fn>
    void ForEachLitFurnace(Fn&& fn) const {
        for (const auto& [pos, state] : m_furnaces) {
            if (GetBlock(pos.x, pos.y, pos.z) == blocks::LitFurnace) fn(pos);
        }
    }

    // Packed light at a world position (sky 15 above the world, 0 when
    // unloaded/below). Used for entity lighting too.
    uint8_t PackedLightAt(const glm::ivec3& worldPos) const;

    // World-space block query; air for unloaded chunks or outside the world.
    BlockId GetBlock(int wx, int wy, int wz) const;
    // M24 per-cell orientation/state meta; 0 for unloaded/outside.
    uint8_t GetMeta(int wx, int wy, int wz) const;

    bool IsSolid(int wx, int wy, int wz) const;

    // M28: a collision box in cell-local coordinates (0..1 within the cell).
    struct BlockBox {
        glm::vec3 min;
        glm::vec3 max;
    };
    // Fills the collision boxes of the block at (wx,wy,wz) and returns the
    // count: 0 for non-solid (air/liquid/plant/torch), 1 for a full cube or a
    // slab half, 2 for a straight stair. Cell-local (0..1) — add the integer
    // cell origin for world space. Drives the player's partial-block collision
    // and auto-step.
    int CollisionBoxesAt(int wx, int wy, int wz, BlockBox out[2]) const;

    // Copy-on-write block edit. Marks the chunk (and, for border blocks,
    // the affected neighbors) for remeshing and dirties every column whose
    // light the edit can reach. No-op outside the world or in ungenerated
    // chunks. The meta overload also writes the cell's M24 orientation byte
    // (default 0 = unoriented); the COW clone carries meta for free.
    void SetBlock(const glm::ivec3& worldPos, BlockId id) { SetBlock(worldPos, id, 0); }
    void SetBlock(const glm::ivec3& worldPos, BlockId id, uint8_t meta);

    struct RaycastHit {
        glm::ivec3 block;
        glm::ivec3 normal; // unit axis vector of the face that was hit
        glm::vec3 point;   // M28: exact world-space hit point (slab/stair half)
    };

    // Amanatides & Woo voxel walk to the first targetable block. Returns
    // nothing if the ray starts inside one or exhausts maxDistance.
    // `includeLiquids` also stops on liquid cells (for the bucket: pick up
    // the liquid you're aiming at, which the normal crosshair ray skips).
    std::optional<RaycastHit> RaycastBlocks(const glm::vec3& origin, const glm::vec3& dir,
                                            float maxDistance, bool includeLiquids = false) const;

    const Chunk* GetChunk(const glm::ivec3& chunkCoord) const;

    // Builds the frame's draw lists. With occlusion on, BFS-walks the chunk
    // grid from the eye through each chunk's face-connectivity bits
    // (Minecraft-style cave culling) so chunks sealed behind terrain are
    // skipped; chunks without visibility data yet are traversed
    // permissively (over-draw, never false culling). The frustum gates
    // drawing, not traversal. With occlusion off: plain frustum cull of
    // everything. Opaque draw order is the BFS order — roughly
    // front-to-back; outTransparent (liquid meshes) comes back sorted
    // back-to-front by chunk center for the blended pass.
    void CollectVisibleChunks(const glm::vec3& eye, const vox::Frustum& frustum, bool occlusion,
                              std::vector<vox::MeshPool::DrawItem>& out,
                              std::vector<vox::MeshPool::DrawItem>& outTransparent,
                              std::vector<vox::MeshPool::DrawItem>& outModel);

    template <typename Fn> // fn(chunkCoord, meshHandle, indexCount)
    void ForEachRenderableChunk(Fn&& fn) const {
        for (const auto& [coord, entry] : m_chunks) {
            if (entry.mesh != vox::MeshPool::kInvalidMesh) {
                fn(coord, entry.mesh, entry.indexCount);
            }
        }
    }

    // All cubic chunk meshes live here; the renderer draws them in one
    // glMultiDrawElementsIndirect via Meshes().Draw(items).
    vox::MeshPool& Meshes() { return m_meshPool; }
    // Non-cube box geometry (torches; slabs/stairs later) — a separate pool
    // because its vertex layout (float pos + UV) differs from the cubic one.
    vox::MeshPool& ModelMeshes() { return m_modelPool; }

    // The on-disk store (player state lives in its manifest). Main thread only.
    WorldSave& SaveStore() { return m_save; }

    size_t LoadedChunkCount() const { return m_chunks.size(); }
    size_t PendingMeshCount() const { return m_pendingMeshes; }
    size_t JobsInFlight() const { return m_jobsInFlight; }

private:
    struct IVec3Hash {
        size_t operator()(const glm::ivec3& v) const {
            // Large-prime mix; fine for chunk coordinates.
            return static_cast<size_t>(v.x) * 73856093u ^ static_cast<size_t>(v.y) * 19349663u ^
                   static_cast<size_t>(v.z) * 83492791u;
        }
    };
    struct IVec2Hash {
        size_t operator()(const glm::ivec2& v) const {
            return static_cast<size_t>(v.x) * 73856093u ^ static_cast<size_t>(v.y) * 83492791u;
        }
    };
    using ChunkMap = std::unordered_map<glm::ivec3, ChunkEntry, IVec3Hash>;
    using ColumnMap = std::unordered_map<glm::ivec2, ColumnEntry, IVec2Hash>;
    using LodColumnMap = std::unordered_map<glm::ivec2, LodColumnEntry, IVec2Hash>;

    struct GenResult {
        glm::ivec3 coord;
        std::shared_ptr<const Chunk> blocks;
    };
    struct LightResult {
        glm::ivec2 column;
        uint32_t version; // ColumnEntry::dirtySeq the input was captured at
        std::array<std::shared_ptr<const ChunkLight>, kWorldHeightChunks> light;
    };
    struct MeshResult {
        glm::ivec3 coord;
        uint32_t version; // dataVersion the snapshot was taken at
        ChunkMesh mesh;
    };
    struct LodGenResult {
        glm::ivec2 column; // LOD-grid units
        std::array<std::shared_ptr<const Chunk>, kLodHeightChunks> cells;
    };
    struct LodMeshResult {
        glm::ivec2 column;
        std::array<ChunkMesh, kLodHeightChunks> meshes;
    };

    // centerX/centerZ are the player's chunk coords — completed detail
    // meshes upload nearest-first within the per-frame budget, so a player
    // edit (its own chunk, distance 0) is never starved behind the streaming
    // backlog (the bug where a broken block lingered ~0.5 s during load).
    void DrainCompletedJobs(int centerX, int centerZ);
    void SubmitGenerate(const glm::ivec3& coord);
    void SubmitLight(const glm::ivec2& column);
    void SubmitMesh(const glm::ivec3& coord);
    // LOD jobs: generate downsamples the 16 real chunks under the LOD
    // column; mesh greedy-meshes its two LOD chunks from the 3x3 LOD
    // neighborhood (gated on LodNeighborsReady) with full-bright light.
    void SubmitLodGenerate(const glm::ivec2& lodColumn);
    void SubmitLodMesh(const glm::ivec2& lodColumn);
    bool LodNeighborsReady(const glm::ivec2& lodColumn) const;
    // Frustum-culled LOD shell draws (scale kLodScale), skipping columns
    // fully covered by full-detail terrain. Used by both culling modes.
    void AppendLodDraws(int centerX, int centerZ, const vox::Frustum& frustum,
                        std::vector<vox::MeshPool::DrawItem>& out,
                        std::vector<vox::MeshPool::DrawItem>& outTransparent) const;
    // Are all full-detail chunks under this LOD column meshed? (Drives the
    // detail/LOD handover when the player moves into a LOD area.)
    bool DetailMeshedUnder(const glm::ivec2& lodColumn) const;
    // (Re)allocates the pool slot for one vertex stream; frees the old one
    // first. Called per stream (opaque and transparent).
    void UploadMesh(vox::MeshPool::MeshHandle& handle, uint32_t& indexCount,
                    const std::vector<ChunkVertex>& vertices);
    // Same, for the float model stream / m_modelPool.
    void UploadModelMesh(vox::MeshPool::MeshHandle& handle, uint32_t& indexCount,
                         const std::vector<ModelVertex>& vertices);
    ChunkSnapshot SnapshotFor(const glm::ivec3& coord) const;
    // Mesh gating: all 26 neighbors have blocks, and the whole 3x3x3
    // neighborhood (center included) has light.
    bool NeighborsReady(const glm::ivec3& coord) const;
    // Light gating: the 3x3 column neighborhood is fully generated.
    bool ColumnHasData(const glm::ivec2& column) const;
    // Put()s every loaded edited chunk into the save store (autosave/quit).
    void SaveEditedChunks();
    // One scheduled block update: applies the block's rule (gravity,
    // liquid flow) and lets SetBlock's scheduling carry the cascade.
    void ProcessBlockUpdate(const glm::ivec3& worldPos);
    // Liquid rule, matching Minecraft 1.12's BlockDynamicLiquid: flow
    // cells re-derive their level from their neighbors (which is also how
    // flows recede), two sources over solid/source ground mint a new
    // source, falling beats sideways spread, flows over water never sheet
    // (only sources do), and sideways spread is slope-seeking — it only
    // goes toward the directions nearest a drop within 4 blocks.
    void UpdateLiquid(const glm::ivec3& worldPos, int level);
    // M26: a lava cell touching water solidifies (source -> obsidian, strong
    // flow -> cobblestone). Returns true if it converted this cell.
    bool CheckLavaMixing(const glm::ivec3& worldPos, int level);
    // Min path distance (1-based) through passable cells to a cell with a
    // non-solid floor ("hole"), bounded at maxDist; 1000 when none in range.
    // `source` is the flowing liquid's family, so its own sources block the
    // path but a different liquid (or air) is passable.
    int SlopeDistance(const glm::ivec3& worldPos, int distance, int fromDir, BlockId source,
                      int maxDist) const;
    uint32_t DataVersionAt(const glm::ivec3& worldPos) const;
    // Has the mesh of the chunk containing worldPos caught up to (at
    // least) this dataVersion? True for unloaded chunks.
    bool MeshCaughtUp(const glm::ivec3& worldPos, uint32_t version) const;

    WorldSave m_save; // declared before m_generator: its manifest provides the seed
    TerrainGenerator m_generator; // stateless — shared by all workers
    vox::MeshPool m_meshPool;     // main-thread-only, like all GL
    vox::MeshPool m_modelPool;    // non-cube box geometry (torches)
    ChunkMap m_chunks;
    ColumnMap m_columns;
    LodColumnMap m_lodColumns; // entry exists = gen submitted; cells null until it lands
    size_t m_pendingMeshes = 0; // chunks in radius without an up-to-date mesh
    size_t m_jobsInFlight = 0;  // main-thread counter: ++submit, --drain
    std::chrono::steady_clock::time_point m_lastAutosave;

    // Scheduled block updates, ordered by due tick. Duplicates are fine —
    // updates re-check the world state and no-op.
    struct BlockUpdate {
        uint64_t due;
        glm::ivec3 pos;
    };
    struct LaterFirst {
        bool operator()(const BlockUpdate& a, const BlockUpdate& b) const {
            return a.due > b.due;
        }
    };
    uint64_t m_simTick = 0;
    std::priority_queue<BlockUpdate, std::vector<BlockUpdate>, LaterFirst> m_blockUpdates;
    std::vector<FallingBlock> m_fallingBlocks; // settled back into blocks in ~World
    std::vector<ItemEntity> m_itemEntities;    // not persisted (see ItemEntity)
    // Furnace block entities by world position (main-thread, like the
    // chunk map). Entries whose chunk is unloaded just idle in the map
    // (tiny) and resume ticking when it streams back in.
    std::unordered_map<glm::ivec3, FurnaceState, IVec3Hash> m_furnaces;

    // One 20-TPS step of every loaded furnace: vanilla burn/cook rules
    // plus the lit/unlit block swap (relight rides the normal edit path).
    void TickFurnaces();
    // Snapshot m_furnaces into the save store (autosave/quit companion to
    // SaveEditedChunks).
    void SaveFurnaces();

    // One tick of EntityItem physics for every dropped item (called from
    // Tick): move with axis-separated collision, drag, merge, despawn.
    void TickItemEntities();

    // --- M32 mobs (impl in Mob.cpp) ---------------------------------------
    // Periodic natural spawn attempt around the player (light + caps + ring).
    void SpawnMobs(const MobTickCtx& ctx);
    // Drop a dead mob's loot and queue its death sound (caller erases it).
    void EmitMobDeath(const Mob& mob);
    // Snapshot m_mobs into the save store (autosave/quit companion to
    // SaveFurnaces).
    void SaveMobs();
    std::vector<Mob> m_mobs;            // persisted to mobs.dat
    std::vector<MobSound> m_mobSounds;  // drained by GameApp each tick
    uint32_t m_mobTickCount = 0;        // gates the periodic spawn attempt
    // If the replaceable block at pos (plant) is about to be crushed by
    // flowing water or a landing gravity block, pop its drop first.
    void CrushDrops(const glm::ivec3& pos);

    // CollectVisibleChunks scratch, reused across frames. The visit grid
    // covers the view square (stamped instead of cleared); the queue holds
    // the BFS frontier.
    struct VisitNode {
        glm::ivec3 coord;
        uint8_t enterFace; // BlockFace the walk entered through; 6 = seed
        uint8_t dirMask;   // directions stepped on this path, BlockFace bits
    };
    std::vector<uint32_t> m_visitGrid;
    uint32_t m_visitStamp = 0;
    std::vector<VisitNode> m_bfsQueue;

    std::mutex m_completedMutex; // guards the result queues
    std::vector<GenResult> m_completedGen;
    std::vector<LightResult> m_completedLight;
    std::vector<MeshResult> m_completedMesh;
    std::vector<LodGenResult> m_completedLodGen;
    std::vector<LodMeshResult> m_completedLodMesh;

    // Mesh results past the frame's GPU upload budget, carried over and
    // processed first next frame (main-thread only, already counted out
    // of m_jobsInFlight).
    std::vector<MeshResult> m_deferredMesh;
    std::vector<LodMeshResult> m_deferredLodMesh;

    // Declared last so it is destroyed first: joining the workers before
    // the queues/maps/generator they reference go away.
    vox::ThreadPool m_pool;
};

} // namespace vc
