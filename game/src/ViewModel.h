#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "vox/renderer/Camera.h"
#include "vox/renderer/Shader.h"
#include "vox/renderer/Texture.h"
#include "vox/renderer/VertexArray.h"

#include "Inventory.h"

namespace vc {

// First-person view model (M20): the held block (mini cube) or item
// (flat sprite), or the bare arm when empty-handed — animated with
// vanilla ItemRenderer's first-person curves: a 6-tick swing (looped
// while digging) and the equip dip when the hotbar selection changes.
// Tick-simulated, render-interpolated; drawn over a cleared depth buffer
// so the hand never clips into walls.
class ViewModel {
public:
    // Shares GameApp's unit-cube and sprite-quad VAOs (same vertex
    // layout); builds the arm mesh + loads the skin itself (no skin
    // imported -> the empty hand draws nothing, placeholder-safe).
    ViewModel(std::shared_ptr<vox::VertexArray> cube, std::shared_ptr<vox::VertexArray> quad);

    // Starts a swing, or queues the next loop while one is running
    // (digging calls this every frame -> continuous swinging).
    void TriggerSwing() { m_swingQueued = true; }

    // 20 TPS: advances swing/equip; held drives the equip-change dip.
    void Tick(const ItemStack& held);

    // After the world (and a depth clear): light is sky/block at the eye.
    void Render(const vox::PerspectiveCamera& camera, double alpha, glm::vec2 light,
                float sunLight, const glm::vec3& skyTint);

private:
    std::shared_ptr<vox::Shader> m_shader;
    std::shared_ptr<vox::VertexArray> m_cube;
    std::shared_ptr<vox::VertexArray> m_quad;
    std::shared_ptr<vox::VertexArray> m_arm; // steve right arm, skin-textured
    std::shared_ptr<vox::Texture2D> m_skin;  // null without assets/mc

    // Vanilla swing: 6 ticks, progress 0..1, sqrt/sin shaped in Render.
    int m_swingTicks = -1; // -1 = idle
    bool m_swingQueued = false;
    float m_progress = 0.0f, m_prevProgress = 0.0f;
    // Vanilla equip: +-0.4/tick toward (same item ? 1 : 0); the displayed
    // item swaps at the bottom of the dip.
    float m_equip = 0.0f, m_prevEquip = 0.0f;
    ItemId m_displayId = 0;
};

} // namespace vc
