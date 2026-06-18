#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "entity/MobModel.h"
#include "vox/renderer/BoxModel.h"

namespace vc {

// A Steve-style biped built on vox::BoxModel: head (+ hat overlay), body, two
// arms, two legs, authored from vanilla ModelBiped's pixel boxes and skinned
// from a 64x64 entity skin. The M31 debug Steve uses the default (the player
// skin); M32's zombie reuses it with the zombie skin and the arms-forward pose
// (vanilla ModelZombie); M36's skeleton uses thin (2px) limbs + the bow-aim pose
// (vanilla ModelSkeleton). The animation is a port of ModelBiped.setRotationAngles
// (limb swing while walking + the idle arm sway), so it reads like vanilla.
class HumanoidModel : public IMobModel {
public:
    // Arm/idle pose: Default (Steve), Zombie (arms straight ahead), or BowAim
    // (both arms raised toward the target, holding a bow — vanilla ModelSkeleton
    // aiming). BowAim is gated per-frame by the mob's `aiming` flag via the M36
    // variant hook, so a not-yet-firing skeleton stands with arms down.
    enum class Pose { Default, Zombie, BowAim };

    // skinRel is an asset-relative path (e.g. "mc/textures/entity/zombie/zombie.png").
    // M33: inflate grows every box (the armor layers are inflated bipeds — 1.0
    // for the helmet/chest/boots layer, 0.5 for the leggings layer); texW/texH
    // give the skin's dimensions (armor layer textures are 64x32); includeHat
    // drops the head overlay box for armor (it has no second layer). M36:
    // thinArms builds the skeleton's 2px arms/legs instead of the 4px biped ones.
    explicit HumanoidModel(std::string skinRel = "mc/textures/entity/steve.png",
                           Pose pose = Pose::Default, float inflate = 0.0f, float texW = 64.0f,
                           float texH = 64.0f, bool includeHat = true, bool thinArms = false);

    // The bow-aim pose is shown only while aiming (a skeleton mid-draw); the M36
    // mob render path sets this from the mob's runtime `aiming` flag through the
    // IMobModel variant hook. Ignored unless the model was built with Pose::BowAim.
    void SetVariant(int variant) override { m_aiming = variant != 0; }

    // The six biped parts, for per-part show/hide on the armor layers.
    enum class Part { Head, Body, RightArm, LeftArm, RightLeg, LeftLeg };
    void SetVisible(Part part, bool visible);

    // True once the skin loaded (needs the gitignored mc asset overlay; a
    // clean clone without it skips drawing, like the first-person arm).
    bool Ready() const override { return m_model.HasSkin(); }

    // Sets every part's rotation for this frame. limbSwing/limbSwingAmount are
    // the walk-cycle phase/intensity (vanilla EntityLivingBase fields); age is
    // the entity's age in ticks (drives the idle sway); head yaw/pitch are in
    // DEGREES relative to the body.
    void SetRotationAngles(float limbSwing, float limbSwingAmount, float age, float headYawDeg,
                           float headPitchDeg) override;

    // Draws via the bound entity_model shader. modelToWorld maps the Y-down
    // pixel model space to world space (feet on the ground, facing bodyYaw).
    void Render(vox::Shader& shader, const glm::mat4& modelToWorld) const override {
        m_model.Render(shader, modelToWorld);
    }

    // World transform of the right-arm joint (M36: attach the held bow to the
    // skeleton's hand). modelToWorld is the same matrix passed to Render.
    glm::mat4 RightArmTransform(const glm::mat4& modelToWorld) const {
        return m_model.PartTransform(m_rightArm, modelToWorld);
    }

private:
    Pose m_pose = Pose::Default;
    bool m_aiming = false; // BowAim: shown only while the skeleton is drawing
    vox::BoxModel m_model;
    int m_head = -1;
    int m_body = -1;
    int m_rightArm = -1;
    int m_leftArm = -1;
    int m_rightLeg = -1;
    int m_leftLeg = -1;
};

} // namespace vc
