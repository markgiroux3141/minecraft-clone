#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

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

// One axis-aligned box of a block model, mirroring a vanilla model
// "element": a cuboid from/to in 1/16-block "pixel" units (0..16) with a
// per-face texture layer + UV sub-rect. Blocks whose shape isn't a full
// cube (torches now, slabs/stairs/fences/panes later) carry a list of
// these in BlockDef::model; the mesher emits them into the float-position
// "model" vertex stream (see ModelVertex in ChunkMesher.h) instead of the
// packed cubic stream, so the geometry needs no bit-stealing in the packed
// format. UVs are in the same 0..16 pixel units, v measured UP (the side
// face of a bottom slab samples the tile's lower half), matching the cube
// mesher's orientation. A face with on=false is not emitted (interior /
// hidden faces).
struct ModelBox {
    struct Face {
        bool on = false;
        uint16_t tile = 0;                       // texture-array layer
        glm::vec4 uv{0.0f, 0.0f, 16.0f, 16.0f};  // u0, v0, u1, v1 in 0..16 px
    };
    glm::vec3 from{0.0f}; // 0..16
    glm::vec3 to{16.0f};  // 0..16
    std::array<Face, 6> faces{}; // BlockFace order
};

struct BlockDef {
    std::string name;
    bool opaque = true;   // hides adjacent faces
    bool solid = true;    // collision (used from M6)
    bool cutout = false;  // non-opaque cube meshed normally; texture alpha holes
                          // are alpha-tested away in chunk.frag (leaves)
    bool cross = false;   // plant: two diagonal alpha-tested quad pairs instead of
                          // a cube; targetable by raycast but never collides
    bool torch = false;   // gameplay flag: targetable, never collides, needs
                          // solid ground, washed away by water. The shape
                          // lives in `model` (the float model stream), not here.
    bool replaceable = false; // liquids flow into it and falling blocks crush it
                              // (tall grass, flowers — destroyed, not blocking)
    bool liquid = false;  // meshed with corner-sampled surface heights; liquid-liquid faces cull
    // Liquids only: render in the OPAQUE pass (depth-write on, back-face cull
    // on) instead of the blended transparent pass. Lava is fully opaque, so
    // the transparent pass's no-depth-write / no-cull setup made it look
    // inside-out; the opaque pass sorts it correctly. Water stays false
    // (it's translucent and needs the blended back-to-front pass).
    bool liquidOpaque = false;
    bool gravity = false; // falls when unsupported (block-update ticks)
    bool unbreakable = false; // the break edit refuses it (bedrock; no
                              // hardness system, so a simple guard)
    // M24: this cube remembers a horizontal FRONT direction in its per-cell
    // meta (furnace, crafting table). The canonical front tile is
    // faceTiles[PosX] and the side tile is faceTiles[NegX]; the mesher draws
    // the front tile on whatever horizontal face the meta names (meta 0 =
    // PosX, matching pre-M24 saves) and the side tile on the other three.
    bool horizontalFacing = false;
    // M28: half-height slab. The SHAPE (one bottom/top half box) is built by
    // the mesher from the per-cell meta (0 = bottom, nonzero = top), not from
    // `model`; collision is a matching half box (World::CollisionBoxesAt).
    // faceTiles are the base material's; `slabBase` names the full block two
    // matching slabs merge into on placement.
    bool slab = false;
    BlockId slabBase = 0;
    // M28: straight stair (no auto-corner shaping). The SHAPE (a half slab +
    // a quarter box on the facing side) is built by the mesher from meta:
    // low 3 bits = the horizontal BlockFace the stair faces (its tall side =
    // the placer's look direction), bit 3 (8) = upside-down. See facing::
    // StairsMeta. Collision mirrors the two boxes.
    bool stairs = false;
    // RS1 redstone. `redstone` marks a power-network component (wire, lever,
    // redstone block, lamp) so World::ProcessBlockUpdate dispatches the cell
    // (and cells next to one) to the RedstoneEngine. `wireOverlay` renders the
    // block as a flat power-tinted "+" cross on the cell top (power 0..15 in
    // meta; the mesher samples kRedstoneWireTile0 + power) and pops without a
    // solid block below, like a floor torch. `lever` renders the floor lever
    // model (a cobble base + a handle that tilts with the on/off meta bit) and
    // is toggled by RMB. Both are non-solid but targetable (RaycastBlocks).
    bool redstone = false;
    bool wireOverlay = false;
    bool lever = false;
    // Hidden from the creative palette (PaletteIds): internal blocks the player
    // never holds directly — the redstone wire (placed via the dust item) and
    // the lit redstone lamp (an engine-driven swap of the off lamp).
    bool hiddenItem = false;
    // Skylight attenuation for non-opaque blocks, 0..15: blocks direct
    // (straight-down) sky and costs max(1, lightOpacity) per propagation
    // step. Ignored when opaque (opaque always blocks fully). Leaves 1,
    // water 3 — vanilla's values.
    uint8_t lightOpacity = 0;
    // Liquids only: 8 = source, 7..1 = flow strength (decays away from the
    // source; 0 on everything else). Drives the spread/recede block updates.
    uint8_t liquidLevel = 0;
    // Liquids only (M26): the canonical SOURCE id of this liquid's family
    // (water source for water + its flows, lava source for lava + its flows).
    // 0 on non-liquids. The flow engine compares this to tell water from lava
    // — only same-family neighbors feed a cell's level, and a different-family
    // neighbor triggers the lava/water mixing rules instead of flowing.
    BlockId liquidSource = 0;
    uint8_t emission = 0; // block light emitted, 0..15
    // Vanilla hardness: bare-hand break time is hardness * 1.5 seconds
    // (digSpeed 1 / hardness / 30 damage per tick at 20 TPS). 0 breaks
    // instantly (plants). Unbreakable blocks use the flag, not -1.
    float hardness = 1.0f;
    // M35 explosion resistance (vanilla getExplosionResistance = resistance/5):
    // how strongly the block absorbs a blast ray. 0 = derive from hardness (see
    // BlastResistance()); set explicitly for the blocks that must resist
    // (obsidian 1200). The explosion routine skips `unbreakable` and `liquid`
    // blocks outright, so those need no value here.
    float blastResistance = 0.0f;
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
    // Non-cube geometry, in the float model stream. Empty for ordinary
    // cubes/cutouts/crosses/liquids; non-empty makes the mesher emit these
    // boxes instead of a greedy-merged cube (torch; slabs/stairs later).
    std::vector<ModelBox> model;

    uint16_t ResolveDrop(BlockId self) const { return drop == kDropSelf ? self : drop; }

    // M35: blast resistance used by the explosion ray; 0 means derive from
    // hardness (a sensible default for the soft-but-not-special blocks).
    float BlastResistance() const { return blastResistance > 0.0f ? blastResistance : hardness; }

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
// M26 lava: source block (liquidLevel 8) — worldgen pools it on deep cave
// floors (below y10), and it's placeable from the creative palette. Emits
// light 15; spreads slower and shorter than water (decay 2, slow tick).
extern BlockId Lava;
// Flowing lava by strength: LavaFlows[level - 1] for levels 1..7 (mirrors
// WaterFlows). Internal — never shown in the palette, never in saves' hands.
extern std::array<BlockId, 7> LavaFlows;
// M26: lava + water make obsidian (lava source) or stone/cobble (flow);
// our obsidian harvests with an iron pickaxe (no diamond tier yet).
extern BlockId Obsidian;
// M28: slabs + straight stairs for stone / cobblestone / planks / sandstone.
// Each reuses its base block's textures and material props; the shape lives
// in the mesher + collision (driven by per-cell meta). Never generated by
// terrain — crafted (3-in-a-row -> 6 slabs; stair pattern -> 4 stairs).
extern BlockId StoneSlab;
extern BlockId CobbleSlab;
extern BlockId PlankSlab;
extern BlockId SandstoneSlab;
extern BlockId StoneStairs;
extern BlockId CobbleStairs;
extern BlockId PlankStairs;
extern BlockId SandstoneStairs;
// M34: white wool — a sheep drop (and shearing yield). Full opaque cube, soft
// "cloth" material; colored variants/dyeing are a later (farming) milestone.
extern BlockId WhiteWool;
// M35: TNT — placed, then RMB'd with flint & steel to prime it into an
// EntityManager::PrimedTnt that detonates after a fuse. Different top/side/
// bottom tiles (like a log); zero blast resistance (chain-removable). Never
// generated; crafted from gunpowder + sand.
extern BlockId Tnt;

// RS1 redstone (power core: lever -> dust -> lamp). Append-only, like every
// block. RedstoneOre generates deep underground (drops the dust item);
// RedstoneBlock is a constant power-15 source; the lamp is a lit/unlit pair
// swapped by the engine (like the furnace); Lever is a toggle source; Wire is
// the flat dust overlay (power in meta). Wire + LampOn are hiddenItem.
extern BlockId RedstoneOre;
extern BlockId RedstoneBlock;
extern BlockId RedstoneLampOff;
extern BlockId RedstoneLampOn;
extern BlockId Lever;
extern BlockId RedstoneWire;

// RS1 wire power ramp: 16 consecutive atlas tiles, sampled as
// kRedstoneWireTile0 + power (0..15), baked dark->bright red at import. Keep
// BOTH atlas scripts in sync with this (the first layer after the M39 drops).
inline constexpr uint16_t kRedstoneWireTile0 = 129;

// M18 crack overlay: destroy_stage_0..9 occupy ten consecutive texture
// layers right after the block tiles — keep BOTH scripts/gen_textures.py
// and scripts/import_mc_assets.py in sync with this.
inline constexpr uint16_t kFirstCrackTile = 29;

void RegisterDefaults();

} // namespace blocks

// M24 block orientation. Per-cell meta stores a facing; the meaning is
// block-specific (vanilla does the same with its meta int):
//   - horizontalFacing cubes (furnace, crafting table): meta = the BlockFace
//     of the FRONT, one of PosX/NegX/PosZ/NegZ. meta 0 = PosX, so untouched
//     pre-M24 blocks keep their old +X front.
//   - torch: meta 0 = floor (standing); meta = TorchWallMeta(face) for a
//     torch mounted on a wall, where `face` is the horizontal direction the
//     torch points (away from the wall it hangs on).
namespace facing {

// Unit step of a BlockFace direction.
inline constexpr glm::ivec3 Dir(BlockFace f) {
    switch (f) {
    case BlockFace::PosX: return {1, 0, 0};
    case BlockFace::NegX: return {-1, 0, 0};
    case BlockFace::PosY: return {0, 1, 0};
    case BlockFace::NegY: return {0, -1, 0};
    case BlockFace::PosZ: return {0, 0, 1};
    case BlockFace::NegZ: return {0, 0, -1};
    }
    return {0, 0, 0};
}

inline constexpr BlockFace Opposite(BlockFace f) {
    switch (f) {
    case BlockFace::PosX: return BlockFace::NegX;
    case BlockFace::NegX: return BlockFace::PosX;
    case BlockFace::PosY: return BlockFace::NegY;
    case BlockFace::NegY: return BlockFace::PosY;
    case BlockFace::PosZ: return BlockFace::NegZ;
    case BlockFace::NegZ: return BlockFace::PosZ;
    }
    return f;
}

// The horizontal face a look/forward vector points along, snapped to the
// dominant cardinal axis (vanilla EnumFacing.fromAngle).
inline BlockFace HorizontalFromLook(const glm::vec3& look) {
    if (std::abs(look.x) >= std::abs(look.z)) {
        return look.x >= 0.0f ? BlockFace::PosX : BlockFace::NegX;
    }
    return look.z >= 0.0f ? BlockFace::PosZ : BlockFace::NegZ;
}

// Torch meta packing: floor = 0, wall = facing index + 1 (so meta 0 stays
// the floor torch that pre-M24 saves and worldgen produce).
inline constexpr uint8_t TorchFloor = 0;
inline constexpr uint8_t TorchWallMeta(BlockFace pointDir) {
    return static_cast<uint8_t>(static_cast<int>(pointDir) + 1);
}
inline constexpr bool TorchIsWall(uint8_t meta) { return meta != TorchFloor; }
inline constexpr BlockFace TorchWallFacing(uint8_t meta) {
    return static_cast<BlockFace>(meta - 1);
}

// M28 slab meta: 0 = bottom half, nonzero = top half.
inline constexpr uint8_t SlabBottom = 0;
inline constexpr uint8_t SlabTopMeta = 1;
inline constexpr bool SlabIsTop(uint8_t meta) { return meta != SlabBottom; }

// M28 stair meta: low 3 bits = the horizontal BlockFace the stair faces (its
// tall back / the placer's look direction); bit 3 (value 8) = upside-down
// (top half). A stair only stores a horizontal facing, so 3 bits suffice and
// the high bit never collides with a BlockFace value (max 5).
inline constexpr uint8_t StairsMeta(BlockFace face, bool top) {
    return static_cast<uint8_t>(static_cast<int>(face) | (top ? 8 : 0));
}
inline constexpr BlockFace StairsFacing(uint8_t meta) {
    return static_cast<BlockFace>(meta & 0x7);
}
inline constexpr bool StairsIsTop(uint8_t meta) { return (meta & 8) != 0; }

// RS1 redstone wire meta: the cell's power level 0..15 lives in the low nibble,
// so a power change is a SetMeta + remesh (the mesher samples a brighter tile).
inline constexpr uint8_t WireMeta(int power) { return static_cast<uint8_t>(power & 0x0F); }
inline constexpr int WirePower(uint8_t meta) { return meta & 0x0F; }

// RS1 lever meta: floor-mounted in v1, so only the on/off state matters — bit 3
// (value 8) = on, leaving the low bits free for a future wall-mount facing.
inline constexpr uint8_t LeverOnBit = 8;
inline constexpr uint8_t LeverMeta(bool on) { return on ? LeverOnBit : 0; }
inline constexpr bool LeverOn(uint8_t meta) { return (meta & LeverOnBit) != 0; }

} // namespace facing

} // namespace vc
