#include "world/LightEngine.h"

#include <algorithm>
#include <vector>

#include "world/Block.h"

namespace vc {

namespace {

// Working volume: the 3x3 column neighborhood, full height. Cells near the
// volume boundary are under-lit (their own neighborhoods are cut off), but
// only the center column is extracted and it sits >= 16 blocks from every
// boundary — beyond the maximum light range of 15.
constexpr int kSpan = 3 * Chunk::kSize;             // 48
constexpr int kHeight = kWorldHeightBlocks;         // 64
constexpr int kCells = kSpan * kSpan * kHeight;

// x, z in [-16, 31], y in [0, 63].
constexpr int CellIndex(int x, int y, int z) {
    return (y * kSpan + (z + Chunk::kSize)) * kSpan + (x + Chunk::kSize);
}

struct Volume {
    std::vector<uint8_t> opacity; // 0 clear, 1..14 attenuating (leaves, water), 15 opaque
    std::vector<uint8_t> emission;
    std::vector<uint8_t> sky;
    std::vector<uint8_t> block;
};

void Fill(const ColumnLightInput& input, Volume& vol) {
    // Registry lookups hoisted into a tiny per-id table.
    const auto& registry = BlockRegistry::Get();
    std::vector<uint8_t> idOpacity(registry.Count());
    std::vector<uint8_t> idEmission(registry.Count());
    for (size_t id = 0; id < registry.Count(); ++id) {
        const BlockDef& def = registry.Def(static_cast<BlockId>(id));
        idOpacity[id] = def.opaque ? 15 : def.lightOpacity;
        idEmission[id] = def.emission;
    }

    for (int y = 0; y < kHeight; ++y) {
        const int cy = y >> 4;
        for (int z = -Chunk::kSize; z < 2 * Chunk::kSize; ++z) {
            for (int x = -Chunk::kSize; x < 2 * Chunk::kSize; ++x) {
                const int slot = ((z >> 4) + 1) * 3 + ((x >> 4) + 1);
                const Chunk* chunk = input.chunks[slot][cy].get();
                const BlockId id =
                    chunk ? chunk->Get(x & 15, y & 15, z & 15) : blocks::Air;
                const int cell = CellIndex(x, y, z);
                vol.opacity[cell] = idOpacity[id];
                vol.emission[cell] = idEmission[id];
            }
        }
    }
}

// FIFO flood fill: stepping into a cell costs max(1, its opacity) — 1
// through clear cells, more through attenuators (leaves, water); opacity
// 15 never conducts. A cell can be re-pushed when a stronger path reaches
// it later; with monotone attenuation this converges quickly.
void Propagate(const Volume& vol, std::vector<uint8_t>& light, std::vector<int>& queue) {
    constexpr int kStrideX = 1;
    constexpr int kStrideZ = kSpan;
    constexpr int kStrideY = kSpan * kSpan;

    for (size_t head = 0; head < queue.size(); ++head) {
        const int cell = queue[head];
        const int level = light[cell];
        if (level <= 1) {
            continue;
        }

        const int x = cell % kSpan;
        const int z = (cell / kSpan) % kSpan;
        const int y = cell / (kSpan * kSpan);
        const auto relax = [&](int neighbor) {
            const int opacity = vol.opacity[neighbor];
            const int spread = level - std::max(1, opacity);
            if (opacity < 15 && spread > 0 && light[neighbor] < spread) {
                light[neighbor] = static_cast<uint8_t>(spread);
                queue.push_back(neighbor);
            }
        };
        if (x > 0) relax(cell - kStrideX);
        if (x < kSpan - 1) relax(cell + kStrideX);
        if (z > 0) relax(cell - kStrideZ);
        if (z < kSpan - 1) relax(cell + kStrideZ);
        if (y > 0) relax(cell - kStrideY);
        if (y < kHeight - 1) relax(cell + kStrideY);
    }
}

} // namespace

std::array<std::shared_ptr<const ChunkLight>, kWorldHeightChunks> LightEngine::ComputeColumn(
    const ColumnLightInput& input) {
    Volume vol;
    vol.opacity.resize(kCells);
    vol.emission.resize(kCells);
    vol.sky.assign(kCells, 0);
    vol.block.assign(kCells, 0);
    Fill(input, vol);

    std::vector<int> queue;
    queue.reserve(kCells / 4);

    // Sky light: everything with a clear view straight up is level 15;
    // the flood fill then carries it sideways and down under overhangs.
    // Any attenuation (leaves, water) ends the direct beam — those cells
    // get their light from the flood fill instead.
    for (int z = -Chunk::kSize; z < 2 * Chunk::kSize; ++z) {
        for (int x = -Chunk::kSize; x < 2 * Chunk::kSize; ++x) {
            for (int y = kHeight - 1; y >= 0; --y) {
                const int cell = CellIndex(x, y, z);
                if (vol.opacity[cell] > 0) {
                    break;
                }
                vol.sky[cell] = 15;
                queue.push_back(cell);
            }
        }
    }
    Propagate(vol, vol.sky, queue);

    // Block light: emissive cells seed at their emission level. Opaque
    // emitters (glowstone) radiate but don't conduct — Propagate only
    // relaxes into transparent neighbors.
    queue.clear();
    for (int cell = 0; cell < kCells; ++cell) {
        if (vol.emission[cell] > vol.block[cell]) {
            vol.block[cell] = vol.emission[cell];
            queue.push_back(cell);
        }
    }
    Propagate(vol, vol.block, queue);

    std::array<std::shared_ptr<const ChunkLight>, kWorldHeightChunks> result;
    for (int cy = 0; cy < kWorldHeightChunks; ++cy) {
        auto section = std::make_shared<ChunkLight>();
        for (int y = 0; y < Chunk::kSize; ++y) {
            for (int z = 0; z < Chunk::kSize; ++z) {
                for (int x = 0; x < Chunk::kSize; ++x) {
                    const int cell = CellIndex(x, cy * Chunk::kSize + y, z);
                    section->Set(x, y, z, ChunkLight::Pack(vol.sky[cell], vol.block[cell]));
                }
            }
        }
        result[cy] = std::move(section);
    }
    return result;
}

} // namespace vc
