#pragma once

#include <glm/glm.hpp>

#include "world/Chunk.h"

namespace vc::ores {

// Places coal and iron ore veins into one chunk (a port of 1.12's
// WorldGenMinable — an ellipsoid swept between two jittered endpoints —
// see the resource note in docs/HANDOFF.md). Pure function of
// (seed, chunkCoord): every chunk replays the veins of the 3x3 origin
// chunks around it (a vein reaches at most ~27 blocks past its origin
// chunk's corner, well inside one neighbor) and writes its own slice, so
// seams match by construction. Veins only ever replace stone.
//
// Vanilla densities rescaled to our 64-block world (surface ~y19-44):
// coal anywhere underground, iron biased low so "dig deeper for iron"
// survives the squashed height range.
//
// Call after the cave carve (carved cells are air, so veins never deposit
// into open caves) and before tree decoration.
void Place(Chunk& chunk, const glm::ivec3& chunkCoord, int seed);

} // namespace vc::ores
