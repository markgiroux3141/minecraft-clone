#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace vox {

class Shader;
class Texture2D;
class VertexArray;

// A jointed multi-cuboid model in the spirit of Minecraft's ModelBase /
// ModelRenderer. Parts are named cuboid groups with a pivot (rotation point)
// and a per-frame rotation; children inherit their parent's transform. A
// single skin texture covers the whole model with the classic per-box UV
// unwrap.
//
// Geometry is authored in vanilla "pixel" units (16 px = 1 block) with the
// model-space Y axis pointing DOWN, exactly like ModelBiped — so vanilla box
// coords, texture offsets, pivots, and animation angles port verbatim. The
// renderer is animation-AGNOSTIC and unit-agnostic: it does no scaling and no
// orientation flip of its own. The caller passes a single modelToWorld matrix
// that maps pixel/Y-down model space into world space (i.e. it folds in the
// 1/16 scale and whatever flip places the feet on the ground), and sets each
// part's rotation before Render(). Both the in-world mob path and the future
// inventory player-doll reuse this with different modelToWorld matrices.
class BoxModel {
public:
    // One cuboid within a part. origin/size are in pixels, part-local (i.e.
    // relative to the part's pivot, matching ModelRenderer.addBox). texOffset
    // is the top-left of the box's UV island in the skin (image pixels).
    // inflate grows the box outward on every side (vanilla "delta" — the hat
    // overlay uses 0.5). mirror reflects the box (and its UV islands) across
    // X, so a left limb can reuse the right limb's texture island.
    struct Box {
        glm::vec3 origin{0.0f};
        glm::vec3 size{0.0f};
        glm::vec2 texOffset{0.0f};
        float inflate = 0.0f;
        bool mirror = false;
    };

    // Registers a part and returns its index. `parent` must already exist
    // (add parents before children) or be -1 for a root part.
    int AddPart(std::string name, glm::vec3 pivot, std::vector<Box> boxes, int parent = -1);

    int FindPart(std::string_view name) const;

    // Per-frame articulation, radians, in the part's local Y-down frame
    // (vanilla rotateAngleX/Y/Z). Applied in vanilla order Z, then Y, then X.
    void SetRotation(int part, glm::vec3 radians);

    // Hide/show a part (vanilla ModelRenderer.showModel). Hidden parts and
    // their geometry are skipped in Render — the M33 armor layers render only
    // the parts the worn piece covers. Default visible.
    void SetVisible(int part, bool visible);

    // Skin texture + its dimensions in pixels (vanilla textureWidth/Height,
    // usually 64x64). Required for the box UV unwrap.
    void SetSkin(std::shared_ptr<Texture2D> skin, float texWidth, float texHeight);
    bool HasSkin() const { return m_skin != nullptr; }

    // Bakes every part's geometry into GL buffers. Call once after all parts
    // are added and the skin size is set.
    void Build();

    // Draws every part. The caller must have bound `shader` and set its
    // view/projection + lighting uniforms first; Render sets `u_model` per
    // part (modelToWorld * the part's accumulated local transform) and binds
    // the skin to texture unit `skinUnit`. Draws with whatever blend/cull
    // state the caller set (the entity pass uses cull off).
    void Render(Shader& shader, const glm::mat4& modelToWorld, int skinUnit = 1) const;

    // The accumulated world transform of one part (modelToWorld * the chain of
    // pivot/rotation matrices up the parent tree) — used to hang a separate item
    // on an animated joint, e.g. the bow in the skeleton's hand (M36). Returns
    // modelToWorld for an invalid index.
    glm::mat4 PartTransform(int part, const glm::mat4& modelToWorld) const;

private:
    // Forward pass computing every part's accumulated transform (parents precede
    // children, so one pass suffices). Shared by Render + PartTransform.
    std::vector<glm::mat4> AccumulateTransforms(const glm::mat4& modelToWorld) const;

    struct Part {
        std::string name;
        glm::vec3 pivot{0.0f};
        glm::vec3 rotation{0.0f};
        int parent = -1;
        bool visible = true;
        std::vector<Box> boxes;
        std::shared_ptr<VertexArray> vao;
        uint32_t indexCount = 0;
    };

    std::vector<Part> m_parts;
    std::shared_ptr<Texture2D> m_skin;
    float m_texWidth = 64.0f;
    float m_texHeight = 64.0f;
};

} // namespace vox
