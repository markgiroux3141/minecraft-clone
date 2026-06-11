#pragma once

#include <span>

#include <glm/glm.hpp>

#include "world/Block.h"

namespace vox {
class UiRenderer;
}

namespace vc {

// In-game overlay: crosshair + hotbar. Stateless — layout is recomputed from
// the screen size every frame, scaled in integer steps (Minecraft GUI scale).
class Hud {
public:
    static void Draw(vox::UiRenderer& ui, glm::vec2 screen, std::span<const BlockId> hotbar,
                     size_t selectedSlot);
};

} // namespace vc
