#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Item.h"

namespace vox {
class Framebuffer;
class Shader;
class Texture2D;
class Texture2DArray;
class VertexArray;
} // namespace vox

namespace vc {

// Bakes a sheet of 3D block icons (the vanilla GUI iso view: rotate
// [30,225,0], scale 0.625, per-face shading) into an offscreen texture, so the
// 2D HUD can blit them through UiRenderer::DrawImage like any sprite. Only
// solid cubes / slabs / stairs get an icon; flat-sprite items and plants are
// left to the caller's flat-tile path (IconRect returns false for them). The
// sheet is rendered at the slot's on-screen pixel size and re-rendered only
// when the GUI scale changes, so icons stay pixel-crisp at 1:1.
class BlockIcons {
public:
    explicit BlockIcons(std::shared_ptr<vox::Texture2DArray> atlas);
    ~BlockIcons();

    // Re-bakes the sheet if cellPx changed since the last call. Binds and
    // restores the default framebuffer; the caller must reset its own viewport
    // afterwards (pass the window size so this can do it).
    void EnsureBuilt(int cellPx, uint32_t windowWidth, uint32_t windowHeight);

    // True if id renders as a 3D block icon, filling srcPos/srcSize with its
    // cell in the sheet (image-pixel coordinates, top-left origin — the
    // convention UiRenderer::DrawImage expects). False for flat-sprite ids.
    bool IconRect(ItemId id, glm::vec2& srcPos, glm::vec2& srcSize) const;

    // The baked sheet (null before the first EnsureBuilt).
    const std::shared_ptr<vox::Texture2D>& Sheet() const;

    // Whether id is drawn as a 3D block (vs a flat sprite).
    static bool Has3dIcon(ItemId id);

private:
    void Build(int cellPx);

    std::shared_ptr<vox::Texture2DArray> m_atlas;
    std::shared_ptr<vox::Shader> m_shader;
    std::shared_ptr<vox::VertexArray> m_cube;
    std::shared_ptr<vox::VertexArray> m_slab;
    std::shared_ptr<vox::VertexArray> m_stairs;
    std::unique_ptr<vox::Framebuffer> m_sheet;

    int m_cellPx = 0;
    int m_columns = 0;
    std::vector<int> m_cell; // block id -> sheet cell index, or -1
};

} // namespace vc
