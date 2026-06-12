#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace vc {

// Index into BlockRegistry. 0 is always air. uint16_t leaves room for far
// more block types than we'll need before palette compression lands.
using BlockId = uint16_t;

// Order matters: used to index BlockDef::faceTiles and the mesher's tables.
enum class BlockFace : uint8_t { PosX = 0, NegX, PosY, NegY, PosZ, NegZ };

struct BlockDef {
    std::string name;
    bool opaque = true;   // hides adjacent faces
    bool solid = true;    // collision (used from M6)
    bool cutout = false;  // non-opaque cube meshed normally; texture alpha holes
                          // are alpha-tested away in chunk.frag (leaves)
    bool cross = false;   // plant: two diagonal alpha-tested quad pairs instead of
                          // a cube; targetable by raycast but never collides
    bool replaceable = false; // liquids flow into it and falling blocks crush it
                              // (tall grass, flowers — destroyed, not blocking)
    bool liquid = false;  // meshed into the blended pass; liquid-liquid faces cull
    bool gravity = false; // falls when unsupported (block-update ticks)
    // Skylight attenuation for non-opaque blocks, 0..15: blocks direct
    // (straight-down) sky and costs max(1, lightOpacity) per propagation
    // step. Ignored when opaque (opaque always blocks fully). Leaves 1,
    // water 3 — vanilla's values.
    uint8_t lightOpacity = 0;
    // Liquids only: 8 = source, 7..1 = flow strength (decays away from the
    // source; 0 on everything else). Drives the spread/recede block updates.
    uint8_t liquidLevel = 0;
    uint8_t emission = 0; // block light emitted, 0..15
    std::array<uint16_t, 6> faceTiles{}; // texture-array layer per face

    static BlockDef Uniform(std::string name, uint16_t tile) {
        BlockDef def;
        def.name = std::move(name);
        def.faceTiles.fill(tile);
        return def;
    }
};

class BlockRegistry {
public:
    static BlockRegistry& Get();

    BlockId Register(BlockDef def);
    const BlockDef& Def(BlockId id) const { return m_defs[id]; }
    size_t Count() const { return m_defs.size(); }

private:
    BlockRegistry(); // registers air as id 0
    std::vector<BlockDef> m_defs;
};

// Well-known block ids, assigned by RegisterDefaults() at startup.
namespace blocks {

inline constexpr BlockId Air = 0;
extern BlockId Stone;
extern BlockId Dirt;
extern BlockId Grass;
extern BlockId Glowstone;
extern BlockId Sand;
extern BlockId Log;
extern BlockId Leaves;
extern BlockId Water; // source block (liquidLevel 8); world gen places only this
// Flowing water by strength: WaterFlows[level - 1] for levels 1..7.
extern std::array<BlockId, 7> WaterFlows;
extern BlockId SnowyGrass; // M15 snowy-biome surface (snow top, snowed sides)
// M16 cross-mesh plants: need earth (grass/dirt) underfoot...
extern BlockId TallGrass;
extern BlockId Dandelion;
extern BlockId Poppy;
extern BlockId DeadBush; // ...except the dead bush, which needs sand

void RegisterDefaults();

} // namespace blocks

} // namespace vc
