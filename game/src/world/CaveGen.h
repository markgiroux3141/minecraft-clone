#pragma once

#include <bitset>
#include <functional>

#include <glm/glm.hpp>

#include "world/Chunk.h"

namespace vc::caves {

// Cells the carver touched, over the chunk plus a skirt: +-kPad in x/z
// (tree canopy reach) and kPadDown below (trunk reach). Bits are set from
// sphere GEOMETRY (not whether a block actually changed), so every chunk
// sharing a tree derives the same answer for its ground cell — that is
// what keeps cave-aware tree gating deterministic across seams.
struct CarveMask {
    static constexpr int kPad = 2;
    static constexpr int kPadDown = 8;
    static constexpr int kSizeXZ = Chunk::kSize + 2 * kPad;
    static constexpr int kSizeY = Chunk::kSize + kPadDown;

    // Chunk-local coords; lx/lz in [-kPad, kSize+kPad), ly in [-kPadDown,
    // kSize). Out-of-range queries return false.
    bool Carved(int lx, int ly, int lz) const {
        if (lx < -kPad || lx >= Chunk::kSize + kPad || ly < -kPadDown || ly >= Chunk::kSize ||
            lz < -kPad || lz >= Chunk::kSize + kPad) {
            return false;
        }
        return bits[Index(lx, ly, lz)];
    }

    static size_t Index(int lx, int ly, int lz) {
        return (static_cast<size_t>(ly + kPadDown) * kSizeXZ + (lz + kPad)) * kSizeXZ +
               (lx + kPad);
    }

    std::bitset<static_cast<size_t>(kSizeXZ) * kSizeXZ * kSizeY> bits;
};

// Carves Minecraft-1.12-style worm caves into one chunk (a port of
// MapGenCaves from the local 1.12 source — see the resource note in
// docs/HANDOFF.md). Pure function of (seed, chunkCoord): every chunk
// independently walks the tunnels of all origin chunks within range and
// carves its own slice, so seams match by construction. heightAt(wx, wz)
// is the worldgen surface height, used for the analytic ocean-breach
// test (carved air never touches worldgen water).
//
// Call after the terrain fill and before tree decoration (vanilla order);
// mask (optional) is filled for cave-aware decoration gating.
void Carve(Chunk& chunk, const glm::ivec3& chunkCoord, int seed,
           const std::function<int(int, int)>& heightAt, CarveMask* mask = nullptr);

} // namespace vc::caves
