#pragma once

#include <glm/glm.hpp>

namespace vox {
class Shader;
}

namespace vc {

// The render contract every mob's box-model satisfies, so GameApp's mob pass can
// drive any mob uniformly (M34): set the variant, set the per-frame articulation,
// then Render. Concrete models (HumanoidModel, PigModel, CowModel, SheepModel,
// ChickenModel) each build a vox::BoxModel (or two, for the sheep's wool layer)
// and implement this. The renderer hands a single modelToWorld matrix that maps
// the vanilla Y-down pixel model space to world (feet on the ground, facing the
// body yaw, folding in the 1/16 scale + the per-mob render scale).
class IMobModel {
public:
    virtual ~IMobModel() = default;

    // False until the skin overlay loaded (a clean clone with no gitignored mc
    // assets draws no mob, like the debug Steve / bare-hand arm).
    virtual bool Ready() const = 0;

    // Optional per-mob visual variant. Sheep use it (0 = woolly, 1 = sheared)
    // to toggle the fur layer; default no-op for mobs without variants.
    virtual void SetVariant(int /*variant*/) {}

    // Per-frame articulation: limbSwing/limbSwingAmount drive the walk cycle,
    // age the idle/flap, head yaw/pitch in DEGREES relative to the body.
    virtual void SetRotationAngles(float limbSwing, float limbSwingAmount, float age,
                                   float headYawDeg, float headPitchDeg) = 0;

    // Draws via the bound entity_model shader (caller set view/proj + lighting).
    virtual void Render(vox::Shader& shader, const glm::mat4& modelToWorld) const = 0;
};

} // namespace vc
