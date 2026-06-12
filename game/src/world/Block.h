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
    bool liquid = false;  // meshed into the blended pass; liquid-liquid faces cull
    bool gravity = false; // falls when unsupported (block-update ticks)
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

void RegisterDefaults();

} // namespace blocks

} // namespace vc
