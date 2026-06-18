#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "entity/Entity.h" // Body base for ItemEntity
#include "entity/Mob.h"    // Mob, MobType, MobSound
#include "world/Block.h"   // BlockId, blocks::Air

namespace vc {

// Block/collision lookups are reached through World's public query API, passed
// in at construction — EntityManager never touches chunk internals.
class World;

// Owns the world's free-moving entities — falling blocks and dropped items —
// and steps their simulation each tick. Held by World so World.cpp keeps only
// voxel/chunk concerns; collision and block state are read through the World&
// it is constructed with (GetBlock/IsSolid/SetBlock/...), never the chunk map.
class EntityManager {
public:
    explicit EntityManager(World& world) : m_world(world) {}

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
    // Detach a gravity block at worldPos into an in-flight entity (the caller
    // — World::ProcessBlockUpdate — has already cleared the cell). dataVersion
    // is the cell's edit version the entity waits on before it starts drawing
    // (see FallingBlockVisible).
    void SpawnFallingBlock(const glm::ivec3& worldPos, BlockId id, uint32_t dataVersion);

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
    template <typename Fn>
    void PickupItems(const glm::vec3& boxMin, const glm::vec3& boxMax, Fn&& take) {
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

    // If the replaceable block at pos (plant) is about to be crushed by
    // flowing water or a landing gravity block, pop its drop first. Called
    // from World's liquid-flow rules and the falling-block landing.
    void CrushDrops(const glm::ivec3& pos);

    // --- M35 explosions (TNT + creeper) ------------------------------------
    // A primed TNT block in flight (vanilla EntityTNTPrimed): a full cube with
    // a fuse, gravity 0.04 b/tick^2, drag x0.98 (ground x0.7), that detonates an
    // Explode(4.0) when the fuse runs out. Spawned by RMB'ing a TNT block with
    // flint & steel (GameApp). NOT persisted — vanishes on quit, like dropped
    // items (a rare edge case). Inherits Body (render-interpolated cube center).
    struct PrimedTnt : Body {
        int fuse = 80;     // ticks until detonation (vanilla 80 = 4 s)
        int prevFuse = 80; // for the render flash interpolation
    };
    const std::vector<PrimedTnt>& PrimedTnts() const { return m_primedTnt; }
    // Spawn a primed TNT at `pos` (cube bottom-center) with the given fuse.
    void SpawnPrimedTnt(const glm::vec3& pos, int fuse);

    // The shared explosion (vanilla Explosion.doExplosionA/B): casts the 16x16
    // surface rays from `center`, removing blocks whose blast resistance the ray
    // can punch through (skipping unbreakable + liquid), spawning a fraction of
    // their drops; then damages + knocks back nearby mobs (DamageMob) and the
    // player (the injected damagePlayer callback). Queues an ExplosionEvent for
    // GameApp to play the boom + smoke. MUST NOT be called while m_mobs is being
    // iterated by index (it erases dead mobs) — the creeper path defers it to
    // after TickMobs's loop; the TNT path runs it from Tick() where m_mobs is idle.
    void Explode(const glm::vec3& center, float size);

    // A boom GameApp drains each tick for sound + particles (World/Entity stay
    // audio/particle-free, like MobSoundEvents).
    struct ExplosionEvent {
        glm::vec3 pos;
        float size;
    };
    std::vector<ExplosionEvent>& ExplosionEvents() { return m_explosions; }

    // GameApp injects the player's feet + a damage callback (with knockback)
    // before the tick, so both explosion triggers (creeper in TickMobs, TNT in
    // Tick) can hurt the player while World stays Player-agnostic. Same shape as
    // MobTickCtx's damagePlayer (already gated on Walk mode by GameApp).
    void SetExplosionTargets(const glm::vec3& playerFeet,
                             std::function<void(float dmg, const glm::vec3& fromPos)> damagePlayer) {
        m_playerFeet = playerFeet;
        m_damagePlayer = std::move(damagePlayer);
    }

    // --- M32 mobs ----------------------------------------------------------
    // Mobs stay Player- and audio-agnostic, so mob AI gets the player state
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
    std::optional<size_t> RaycastMob(const glm::vec3& origin, const glm::vec3& dir, float maxDist,
                                     float& outDist) const;
    // Damage a mob (player melee): health, red-flash, knockback away from
    // fromPos, hurt/death sound event; drops loot and removes it at <= 0.
    void DamageMob(size_t index, float amount, const glm::vec3& fromPos);
    // M34: shear a sheep (player RMB with shears). Sets the sheared flag (hides
    // the wool layer) and returns how many wool the harvest yields (1-3); the
    // CALLER grants those straight to the inventory (active tool use — vanilla
    // scatters entities, but a harvest the player triggered is friendlier
    // collected) and wears the shears + plays the sound. Returns 0 for a
    // non-sheep or an already-sheared sheep so RMB falls through.
    int ShearMob(size_t index);
    // M35: force-ignite a creeper (player RMB with flint & steel) — sets its
    // fuse alight so it swells to detonation regardless of player range. Returns
    // true if `index` was a creeper, false for any other mob (so RMB falls
    // through to the normal place/use path).
    bool IgniteMob(size_t index);
    // Mob sounds the sim wants played this tick (hurt/death). GameApp drains
    // and clears it after Tick, keeping the sim audio-free.
    std::vector<MobSound>& MobSoundEvents() { return m_mobSounds; }
    // Load persisted mobs from the world's save store (World ctor) / snapshot
    // them back into it (autosave/quit). The store stays on World; these just
    // marshal m_mobs through it.
    void LoadMobs();
    void SaveMobs();

    // One 20-TPS step of the falling blocks + dropped items (from World::Tick).
    void Tick();
    // World dtor: settle every in-flight block back into a real block so the
    // edit persists across save/quit.
    void SettleFallingBlocks();

private:
    // Falling blocks: accelerate down, settle when the cell below the one
    // being entered is solid.
    void TickFallingBlocks();
    // One tick of EntityItem physics for every dropped item: move with
    // axis-separated collision, drag, merge, despawn.
    void TickItemEntities();
    // One tick of every primed TNT (gravity + drag + collision, fuse countdown,
    // detonate at 0). Detonation runs Explode() — safe here, m_mobs is idle.
    void TickPrimedTnt();
    // M35: line-of-sight blast exposure of `target` from `center` — 1.0 if a
    // single ray reaches it unobstructed, ~0.4 if a block blocks it (vanilla
    // samples the whole AABB; this is the cheap approximation).
    float BlastDensity(const glm::vec3& center, const glm::vec3& target) const;
    // Periodic natural spawn attempt around the player (light + caps + ring).
    void SpawnMobs(const MobTickCtx& ctx);
    // Drop a dead mob's loot and queue its death sound (caller erases it).
    void EmitMobDeath(const Mob& mob);

    World& m_world; // block/collision queries (public API only); outlives this
    std::vector<FallingBlock> m_fallingBlocks; // settled back into blocks on dtor
    std::vector<ItemEntity> m_itemEntities;    // not persisted (see ItemEntity)
    std::vector<PrimedTnt> m_primedTnt;        // not persisted (see PrimedTnt)
    std::vector<Mob> m_mobs;                   // persisted to mobs.dat
    std::vector<MobSound> m_mobSounds;         // drained by GameApp each tick
    std::vector<ExplosionEvent> m_explosions;  // drained by GameApp each tick
    uint32_t m_mobTickCount = 0;               // gates the periodic spawn attempt
    // M35 explosion targets, injected by GameApp before each tick (see
    // SetExplosionTargets) so explosions can hurt the player without World
    // knowing about Player.
    glm::vec3 m_playerFeet{0.0f};
    std::function<void(float dmg, const glm::vec3& fromPos)> m_damagePlayer;
};

} // namespace vc
