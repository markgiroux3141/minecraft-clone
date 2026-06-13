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

// M19: shared by BlockDef (which tool digs this faster) and ItemDef (what
// kind of tool the item is).
enum class ToolClass : uint8_t { None, Pickaxe, Axe, Shovel };

// M22: vanilla Block.StepSound class — picks the dig/break/place/step sound
// set. Names map to the assets/mc/sounds/sound/{dig,step}/<material>N.ogg
// families (cloth/grass/gravel/sand/snow/stone/wood). None = silent (air,
// water). Glass has only a break sound (random/glass); its dig/step/place
// fall back to the stone set (vanilla parity).
enum class SoundType : uint8_t { None, Stone, Wood, Grass, Gravel, Sand, Snow, Cloth, Glass };

struct BlockDef {
    std::string name;
    bool opaque = true;   // hides adjacent faces
    bool solid = true;    // collision (used from M6)
    bool cutout = false;  // non-opaque cube meshed normally; texture alpha holes
                          // are alpha-tested away in chunk.frag (leaves)
    bool cross = false;   // plant: two diagonal alpha-tested quad pairs instead of
                          // a cube; targetable by raycast but never collides
    bool torch = false;   // vanilla torch shape: four one-sided alpha-tested
                          // planes inset 7/16 and 9/16 from the cell walls;
                          // targetable, never collides, needs solid ground
    bool replaceable = false; // liquids flow into it and falling blocks crush it
                              // (tall grass, flowers — destroyed, not blocking)
    bool liquid = false;  // meshed into the blended pass; liquid-liquid faces cull
    bool gravity = false; // falls when unsupported (block-update ticks)
    bool unbreakable = false; // the break edit refuses it (bedrock; no
                              // hardness system, so a simple guard)
    // Skylight attenuation for non-opaque blocks, 0..15: blocks direct
    // (straight-down) sky and costs max(1, lightOpacity) per propagation
    // step. Ignored when opaque (opaque always blocks fully). Leaves 1,
    // water 3 — vanilla's values.
    uint8_t lightOpacity = 0;
    // Liquids only: 8 = source, 7..1 = flow strength (decays away from the
    // source; 0 on everything else). Drives the spread/recede block updates.
    uint8_t liquidLevel = 0;
    uint8_t emission = 0; // block light emitted, 0..15
    // Vanilla hardness: bare-hand break time is hardness * 1.5 seconds
    // (digSpeed 1 / hardness / 30 damage per tick at 20 TPS). 0 breaks
    // instantly (plants). Unbreakable blocks use the flag, not -1.
    float hardness = 1.0f;
    // What breaking one of these yields: kDropSelf (default), Air for
    // nothing (leaves, tall grass), or a specific id. The value is a
    // unified ItemId (see Item.h) — block ids place, higher ids are
    // registry items (coal ore -> the coal item).
    static constexpr BlockId kDropSelf = 0xFFFF;
    uint16_t drop = kDropSelf;
    // M19: the tool class that digs this block at the tool's efficiency
    // multiplier (others dig at 1x).
    ToolClass toolClass = ToolClass::None;
    // Vanilla harvest gating: without a pickaxe in hand this block digs
    // at the /100 rate (3.3x slower) and drops NOTHING (stone family).
    bool needsPickaxe = false;
    // M21 tiers (vanilla harvest levels): the pickaxe's ItemDef::tier must
    // reach this for the block to drop (0 = wood suffices, 1 = stone+,
    // 2 = iron+). Only meaningful when needsPickaxe is set.
    uint8_t harvestLevel = 0;
    // M22: which dig/break/place/step sound set this block uses. Stone is the
    // sensible default (most of the world is stony); helpers/RegisterDefaults
    // override per material.
    SoundType soundType = SoundType::Stone;
    std::array<uint16_t, 6> faceTiles{}; // texture-array layer per face

    uint16_t ResolveDrop(BlockId self) const { return drop == kDropSelf ? self : drop; }

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
    // Registration-time patching only (cross-referencing drops whose target
    // id is registered later, e.g. stone -> cobblestone).
    BlockDef& EditDef(BlockId id) { return m_defs[id]; }
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
// M16 tree species (birch in forests, spruce in snow) + the desert cactus
// (full cube for now; vanilla's 14/16 inset is a later polish).
extern BlockId BirchLog;
extern BlockId BirchLeaves;
extern BlockId SpruceLog;
extern BlockId SpruceLeaves;
extern BlockId Cactus; // needs sand (or more cactus) underfoot
// M16: vanilla's buffer under sandy surfaces — carvable but never falls,
// so caves under deserts/beaches mostly expose sandstone ceilings instead
// of floating sand (Biome.generateBiomeTerrain's sand->sandstone rule).
extern BlockId Sandstone;
extern BlockId Bedrock; // unbreakable world floor (solid y0, ragged y1-4)
// M18: what stone drops when mined (never generated by terrain).
extern BlockId Cobblestone;
// M19 crafting: planks (from any log) and the crafting table (RMB opens
// the 3x3 grid). Neither is generated by terrain.
extern BlockId Planks;
extern BlockId CraftingTable;
// M21 ores (worldgen veins in stone, see OreGen.h): coal drops the coal
// item; iron drops itself and must be smelted (stone pickaxe to harvest).
extern BlockId CoalOre;
extern BlockId IronOre;
// M21 furnace: the lit variant is a separate block id swapped in/out as
// the burn state changes (like the water flow ids); its per-position
// slots/progress live in World's furnace map, not the block.
extern BlockId Furnace;
extern BlockId LitFurnace;
extern BlockId Glass; // smelted from sand; never generated
// M21 follow-up: floor-standing torch (coal + stick), block light 14.
// Wall mounting waits for block orientation data (see backlog).
extern BlockId Torch;

// M18 crack overlay: destroy_stage_0..9 occupy ten consecutive texture
// layers right after the block tiles — keep BOTH scripts/gen_textures.py
// and scripts/import_mc_assets.py in sync with this.
inline constexpr uint16_t kFirstCrackTile = 29;

void RegisterDefaults();

} // namespace blocks

} // namespace vc
