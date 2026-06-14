#pragma once

#include <bitset>
#include <functional>

#include <glm/glm.hpp>

#include "world/CaveGen.h"
#include "world/Chunk.h"

namespace vc::lakes {

// XZ columns a lake claimed, over the chunk plus a +-kPad skirt (tree-canopy
// reach), so decoration can veto trees/plants that would sit in a pond — the
// same role caves::CarveMask plays for cave mouths. Filled from the same
// origin enumeration every chunk runs, so the veto is identical across seams.
struct LakeMask {
    static constexpr int kPad = 2;
    static constexpr int kSizeXZ = Chunk::kSize + 2 * kPad;

    bool InLake(int lx, int lz) const {
        if (lx < -kPad || lx >= Chunk::kSize + kPad || lz < -kPad || lz >= Chunk::kSize + kPad) {
            return false;
        }
        return bits[Index(lx, lz)];
    }
    void Mark(int lx, int lz) {
        if (lx < -kPad || lx >= Chunk::kSize + kPad || lz < -kPad || lz >= Chunk::kSize + kPad) {
            return;
        }
        bits[Index(lx, lz)] = true;
    }
    static size_t Index(int lx, int lz) {
        return static_cast<size_t>(lz + kPad) * kSizeXZ + (lx + kPad);
    }

    std::bitset<static_cast<size_t>(kSizeXZ) * kSizeXZ> bits;
};

// Ports Minecraft 1.12's WorldGenLakes (the scattered surface ponds) as a
// deterministic, chunk-pure populate step. Vanilla runs it sequentially over
// the live world during populate; we make it a pure function of (seed,
// chunkCoord): every chunk replays the lake candidate of each chunk in its
// +-1 neighbourhood, seeded per origin.
//
// Each lake is anchored to its origin chunk's 16x16 footprint (no jitter) and
// its blob only fills interior cells (1..14), so a lake keeps a >=1-column
// margin of solid ground on every side. Adjacent chunks' lakes therefore can
// never touch or overlap — every lake self-seals and there are no seam cuts,
// without any live-world seal check. The blob is also anchored BELOW the
// lowest surface in its footprint, so the basin floor/walls are provably solid
// (caves excepted — a lake over a near-surface cave can rarely leak; accepted).
//
// Each lake's blob is also constrained to fit within a single vertical chunk,
// so one chunk owns and writes it entirely and can reject lakes that intersect
// a cave (using that chunk's carve mask) — keeping water off open cave air
// without any cross-seam coordination.
//
// heightAt(wx, wz) is the worldgen surface; isOcean / isDesert gate placement
// (no lakes in the sea; water lakes skip deserts, like vanilla). carveMask is
// this chunk's caves (for the cave-intersection reject); mask (if given)
// records claimed columns for decoration veto. Call BEFORE trees and plants
// (vanilla populate order) so decoration can avoid the pond.
void Place(Chunk& chunk, const glm::ivec3& chunkCoord, int seed,
           const std::function<int(int, int)>& heightAt,
           const std::function<bool(int, int)>& isOcean,
           const std::function<bool(int, int)>& isDesert, const caves::CarveMask* carveMask,
           LakeMask* mask = nullptr);

} // namespace vc::lakes
