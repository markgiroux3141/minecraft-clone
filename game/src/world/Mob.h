#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "Item.h" // ItemId for mob drops

namespace vc {

// M32: the living entities. One passive (pig) + one hostile (zombie) to
// exercise both AI paths. A Mob is the same tick-simulated, prev/current-
// interpolated, AABB-collided shape as World::FallingBlock / World::ItemEntity
// (the M30/M31 vitals + box-model renderer plug in on top): health, a body AABB
// it resolves against blocks, a skinned vox::BoxModel, and the vanilla
// EntityLivingBase walk-cycle accumulators.
enum class MobType : uint8_t { Pig = 0, Zombie = 1, Count };

// Static per-type tuning (sizes/health/speed/combat). Ported loosely from the
// 1.12 entity classes. Drop ids are NOT here — they're runtime ItemRegistry
// ids (0 until items::RegisterDefaults), resolved by MobDropItem().
struct MobDef {
    float halfWidth;    // AABB half-extent on X/Z
    float height;       // AABB height
    float maxHealth;    // hearts*2
    float speed;        // ground move speed, blocks/s
    bool hostile;       // chases + attacks the player
    float attackDamage; // melee dealt to the player (health points)
    float followRange;  // blocks within which a hostile targets the player
    float modelOffsetPx; // model-space feet offset for the upright flip (px, Y-down)
    int dropMin;        // loot count range
    int dropMax;
};

inline constexpr MobDef kMobDefs[] = {
    // Pig: 0.9x0.9, 10 health, ambles, drops 1-3 porkchops. (Both models' feet
    // sit at model-space y=24px, so the upright-flip offset is 24 for both.)
    {0.45f, 0.9f, 10.0f, 3.4f, false, 0.0f, 0.0f, 24.0f, 1, 3},
    // Zombie: 0.6x1.95, 20 health, hits for 3, drops 0-2. Chase speed 4.0 b/s
    // is just under the player's 4.3 walk (vanilla movementSpeed 0.23 ≈ slightly
    // slower than walking): a walking player slowly pulls ahead, sprinting
    // clearly escapes — instead of the old 4.4 that nothing but sprinting beat.
    {0.3f, 1.95f, 20.0f, 4.0f, true, 3.0f, 16.0f, 24.0f, 0, 2},
};
static_assert(sizeof(kMobDefs) / sizeof(kMobDefs[0]) == static_cast<size_t>(MobType::Count));

inline const MobDef& MobDefOf(MobType type) {
    return kMobDefs[static_cast<size_t>(type)];
}

// The item a mob drops on death (runtime id, so a free function rather than a
// table entry). Air for none.
ItemId MobDropItem(MobType type);

// One living entity in the world. Bottom-center position (feet), like the
// player. Body yaw is in RADIANS (world), facing the direction of travel /
// the target; the model matrix folds in the upright flip.
struct Mob {
    MobType type = MobType::Pig;
    glm::vec3 pos{0.0f};
    glm::vec3 prevPos{0.0f};
    glm::vec3 vel{0.0f};
    float yaw = 0.0f;
    float prevYaw = 0.0f;
    float health = 10.0f;
    bool onGround = false;
    float fallDistance = 0.0f;

    // Vanilla EntityLivingBase walk-cycle accumulators (+ prev for render
    // interpolation) and age (drives idle sway), exactly like the M31 DebugMob.
    float limbSwing = 0.0f;
    float prevLimbSwing = 0.0f;
    float limbSwingAmount = 0.0f;
    float prevLimbSwingAmount = 0.0f;
    float age = 0.0f;

    // Combat / feedback.
    int hurtTime = 0;       // >0 tints the model red and counts down
    int attackCooldown = 0; // ticks until a hostile can swing again

    // AI scratch.
    int aiTimer = 0;          // ticks until the next wander re-decision
    glm::vec2 wanderDir{0.0f}; // current stroll direction (passive idle)
    bool moving = false;       // AI wants to move this tick (drives the walk anim)
};

// A positional sound the mob sim wants played (drained by GameApp so World
// stays audio-free, like ForEachLitFurnace). kind: 0 = hurt, 1 = death.
struct MobSound {
    MobType type;
    glm::vec3 pos;
    uint8_t kind;
};

} // namespace vc
