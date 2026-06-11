#pragma once

#include <array>
#include <memory>

#include "world/Light.h"

namespace vc {

// Everything a column light job needs, captured on the main thread.
// chunks[(dz+1)*3 + (dx+1)][cy] is the 3x3 column neighborhood by height;
// null = ungenerated (treated as air, only possible at the stream edge).
struct ColumnLightInput {
    std::array<std::array<std::shared_ptr<const Chunk>, kWorldHeightChunks>, 9> chunks;
};

class LightEngine {
public:
    // Computes sky + block light for the CENTER column of the input.
    // BFS attenuates 1 per step and caps at 15, so a 3x3 column
    // neighborhood (±16 blocks) provably contains every cell that can
    // influence the center column. Pure function — runs on workers
    // (BlockRegistry is read-only after startup).
    static std::array<std::shared_ptr<const ChunkLight>, kWorldHeightChunks> ComputeColumn(
        const ColumnLightInput& input);
};

} // namespace vc
