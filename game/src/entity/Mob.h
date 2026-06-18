#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "entity/Entity.h" // Body + LivingAnim shared bases
#include "item/Item.h"     // ItemId for mob drops

namespace vc {

// M32: the living entities. One passive (pig) + one hostile (zombie) to
// exercise both AI paths. A Mob is the same tick-simulated, prev/current-
// interpolated, AABB-collided shape as EntityManager::FallingBlock / ItemEntity
// (the M30/M31 vitals + box-model renderer plug in on top): health, a body AABB
// it resolves against blocks, a skinned vox::BoxModel, and the vanilla
// EntityLivingBase walk-cycle accumulators.
enum class MobType : uint8_t { Pig = 0, Zombie = 1, Cow = 2, Sheep = 3, Chicken = 4, Count };

// How a mob is placed by natural spawning (the rule that gates which surface +
// light level it spawns on). Data-driven so a new mob is a table row, not an
// if-branch in SpawnMobs (M34 generalization).
enum class SpawnRule : uint8_t {
    SurfaceDay, // on grass, sky-light >= 9, daytime (passive animals)
    Dark,       // block-light <= 7 and (night or sky-light <= 7) (hostiles)
};

// Static per-type tuning (sizes/health/speed/combat/spawn/render). Ported
// loosely from the 1.12 entity classes. Drop ids are NOT here — they're runtime
// ItemRegistry ids (0 until items::RegisterDefaults), resolved by MobDrops().
struct MobDef {
    float halfWidth;    // AABB half-extent on X/Z
    float height;       // AABB height
    float maxHealth;    // hearts*2
    float speed;        // ground move speed, blocks/s
    bool hostile;       // chases + attacks the player
    float attackDamage; // melee dealt to the player (health points)
    float followRange;  // blocks within which a hostile targets the player
    float modelOffsetPx; // model-space feet offset for the upright flip (px, Y-down)
    float modelScale;    // render multiplier (1.0 normal; babies/variants later)
    SpawnRule spawnRule; // natural-spawn placement category
    int spawnWeight;     // relative weight among mobs sharing this spawn rule
    bool slowFall;       // no fall damage + damped descent (chicken)
    bool laysEggs;       // periodic egg drop (chicken)
};

inline constexpr MobDef kMobDefs[] = {
    // All models' feet sit at model-space y=24px, so modelOffsetPx is 24 for
    // every mob; modelScale 1.0 until baby/variant sizing lands. Speeds are the
    // vanilla movementSpeed attribute scaled to the pig's existing 3.4 b/s
    // (0.25 attr) so passives amble at comparable feel.
    // Pig: 0.9x0.9, 10 hp, drops porkchops. (spawnWeight 10, vanilla)
    {0.45f, 0.9f, 10.0f, 3.4f, false, 0.0f, 0.0f, 24.0f, 1.0f, SpawnRule::SurfaceDay, 10, false,
     false},
    // Zombie: 0.6x1.95, 20 hp, hits for 3. Chase 4.0 b/s is just under the
    // player's 4.3 walk (vanilla 0.23): walk away slowly, sprint to escape.
    {0.3f, 1.95f, 20.0f, 4.0f, true, 3.0f, 16.0f, 24.0f, 1.0f, SpawnRule::Dark, 100, false, false},
    // Cow: 0.9x1.4, 10 hp, slow amble (vanilla 0.20 -> 2.7 b/s); beef + leather.
    {0.45f, 1.4f, 10.0f, 2.7f, false, 0.0f, 0.0f, 24.0f, 1.0f, SpawnRule::SurfaceDay, 8, false,
     false},
    // Sheep: 0.9x1.3, 8 hp (vanilla 0.23 -> 3.1 b/s); mutton + (un-sheared) wool.
    {0.45f, 1.3f, 8.0f, 3.1f, false, 0.0f, 0.0f, 24.0f, 1.0f, SpawnRule::SurfaceDay, 12, false,
     false},
    // Chicken: 0.4x0.7, 4 hp (vanilla 0.25 -> 3.4 b/s); chicken + feathers, lays
    // eggs, takes no fall damage (flutters down).
    {0.2f, 0.7f, 4.0f, 3.4f, false, 0.0f, 0.0f, 24.0f, 1.0f, SpawnRule::SurfaceDay, 10, true, true},
};
static_assert(sizeof(kMobDefs) / sizeof(kMobDefs[0]) == static_cast<size_t>(MobType::Count));

inline const MobDef& MobDefOf(MobType type) {
    return kMobDefs[static_cast<size_t>(type)];
}

// One entry in a mob's death loot: an item id and an inclusive count range.
struct MobDrop {
    ItemId item;
    int min;
    int max;
};

// A mob's death loot (runtime ids, so a function rather than a table). `sheared`
// suppresses the sheep's wool drop. Empty for mobs with no drop.
std::vector<MobDrop> MobDrops(MobType type, bool sheared);

// The sound-family folder under assets/mc/sounds/mob/ for this mob type
// ("pig", "zombie", "cow", "sheep", "chicken"). Drives GameSounds loading.
const char* MobSoundFolder(MobType type);

// One living entity in the world. Bottom-center position (feet), like the
// player. Body yaw (from LivingAnim) is in RADIANS (world), facing the
// direction of travel / the target; the model matrix folds in the upright flip.
// Inherits the render-interpolated position (Body: pos/prevPos/vel) and the
// vanilla walk-cycle accumulators (LivingAnim: yaw/limbSwing*/age) — shared
// with GameApp's DebugMob so they no longer duplicate these fields.
struct Mob : Body, LivingAnim {
    MobType type = MobType::Pig;
    float health = 10.0f;
    bool onGround = false;
    float fallDistance = 0.0f;

    // Combat / feedback.
    int hurtTime = 0;       // >0 tints the model red and counts down
    int attackCooldown = 0; // ticks until a hostile can swing again

    // AI scratch.
    int aiTimer = 0;          // ticks until the next wander re-decision
    glm::vec2 wanderDir{0.0f}; // current stroll direction (passive idle)
    bool moving = false;       // AI wants to move this tick (drives the walk anim)

    // M34 species state (runtime only — not persisted; a reloaded sheep grows
    // its wool back, a chicken's egg cycle restarts).
    bool sheared = false; // sheep: wool harvested (hides the fur layer, no wool drop)
    int eggTimer = 0;     // chicken: ticks until the next egg (vanilla 6000..11999)
};

// A positional sound the mob sim wants played (drained by GameApp so World
// stays audio-free, like ForEachLitFurnace). kind: 0 = hurt, 1 = death,
// 2 = egg-lay (chicken plop).
struct MobSound {
    MobType type;
    glm::vec3 pos;
    uint8_t kind;
};

} // namespace vc
