#pragma once

#include <glm/glm.hpp>

namespace vc {

// Shared building blocks for everything that moves and is drawn between ticks.
// The sim runs at a fixed 20 TPS (OnTick) while rendering is uncapped
// (OnRender) — so any moving body keeps its PREVIOUS and CURRENT tick state and
// the renderer interpolates between them by the frame alpha. Before M-refactor
// these fields were copy-pasted onto Mob, World::ItemEntity and GameApp's
// DebugMob; they now compose these bases instead (see CLAUDE.md "Keeping
// modules small"). World::FallingBlock deliberately opts out — it is
// column-locked 1-D motion (y/prevY only), not a free vec3 body.

// A render-interpolated kinematic body. Position is bottom-center (feet),
// matching the player. Call BeginTick() at the top of each tick (before
// integrating velocity) so prevPos holds last tick's value for interpolation.
struct Body {
    glm::vec3 prevPos{0.0f};
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};

    glm::vec3 RenderPos(float alpha) const { return prevPos + (pos - prevPos) * alpha; }
    void BeginTick() { prevPos = pos; }
};

// Vanilla EntityLivingBase walk-cycle + idle animation state. limbSwing /
// limbSwingAmount drive the model's leg/arm rotation (ModelBiped / quadruped);
// age drives the idle head sway. yaw is body facing in radians (world). prev*
// fields are sampled per tick for render interpolation — call BeginTick() at
// the top of each tick.
struct LivingAnim {
    float yaw = 0.0f;
    float prevYaw = 0.0f;
    float limbSwing = 0.0f;
    float prevLimbSwing = 0.0f;
    float limbSwingAmount = 0.0f;
    float prevLimbSwingAmount = 0.0f;
    float age = 0.0f;

    void BeginTick() {
        prevYaw = yaw;
        prevLimbSwing = limbSwing;
        prevLimbSwingAmount = limbSwingAmount;
    }
};

} // namespace vc
