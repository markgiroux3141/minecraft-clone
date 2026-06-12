#include "world/Block.h"

#include "vox/core/Log.h"

namespace vc {

BlockRegistry& BlockRegistry::Get() {
    static BlockRegistry instance;
    return instance;
}

BlockRegistry::BlockRegistry() {
    BlockDef air;
    air.name = "air";
    air.opaque = false;
    air.solid = false;
    m_defs.push_back(std::move(air));
}

BlockId BlockRegistry::Register(BlockDef def) {
    const auto id = static_cast<BlockId>(m_defs.size());
    m_defs.push_back(std::move(def));
    return id;
}

namespace blocks {

BlockId Stone = 0;
BlockId Dirt = 0;
BlockId Grass = 0;
BlockId Glowstone = 0;
BlockId Sand = 0;
BlockId Log = 0;
BlockId Leaves = 0;
BlockId Water = 0;
std::array<BlockId, 7> WaterFlows{};
BlockId SnowyGrass = 0;
BlockId TallGrass = 0;
BlockId Dandelion = 0;
BlockId Poppy = 0;
BlockId DeadBush = 0;
BlockId BirchLog = 0;
BlockId BirchLeaves = 0;
BlockId SpruceLog = 0;
BlockId SpruceLeaves = 0;
BlockId Cactus = 0;
BlockId Sandstone = 0;
BlockId Bedrock = 0;
BlockId Cobblestone = 0;
BlockId Planks = 0;
BlockId CraftingTable = 0;
BlockId CoalOre = 0;
BlockId IronOre = 0;
BlockId Furnace = 0;
BlockId LitFurnace = 0;
BlockId Glass = 0;

namespace {

BlockDef Plant(std::string name, uint16_t tile) {
    BlockDef def = BlockDef::Uniform(std::move(name), tile);
    def.opaque = false;
    def.solid = false; // walk through; raycast still targets it (cross)
    def.cross = true;
    def.replaceable = true;
    def.hardness = 0.0f; // vanilla: plants break instantly
    return def;
}

BlockDef LogDef(std::string name, uint16_t sideTile, uint16_t topTile) {
    BlockDef def = BlockDef::Uniform(std::move(name), sideTile);
    def.faceTiles[static_cast<size_t>(BlockFace::PosY)] = topTile;
    def.faceTiles[static_cast<size_t>(BlockFace::NegY)] = topTile;
    def.hardness = 2.0f; // vanilla BlockLog
    def.toolClass = ToolClass::Axe;
    return def;
}

BlockDef LeavesDef(std::string name, uint16_t tile) {
    BlockDef def = BlockDef::Uniform(std::move(name), tile);
    def.opaque = false;
    def.cutout = true;
    def.lightOpacity = 1;
    def.hardness = 0.2f;     // vanilla BlockLeaves
    def.drop = blocks::Air;  // nothing without shears/saplings (M18 table)
    return def;
}

} // namespace

void RegisterDefaults() {
    auto& registry = BlockRegistry::Get();
    if (Stone != 0) {
        return; // already registered
    }

    // Tile indices match the layer order in scripts/gen_textures.py:
    // 0 stone, 1 dirt, 2 grass side, 3 grass top, 4 glowstone, 5 sand,
    // 6 log side, 7 log top, 8 leaves. Existing save blobs store BlockIds,
    // so only APPEND registrations — never reorder.
    // M18 hardness values are vanilla's (Block.registerBlocks); M19 tool
    // classes/pickaxe gating follow vanilla's materials.
    BlockDef stone = BlockDef::Uniform("stone", 0);
    stone.hardness = 1.5f;
    stone.toolClass = ToolClass::Pickaxe;
    stone.needsPickaxe = true;
    Stone = registry.Register(std::move(stone));

    BlockDef dirt = BlockDef::Uniform("dirt", 1);
    dirt.hardness = 0.5f;
    dirt.toolClass = ToolClass::Shovel;
    Dirt = registry.Register(std::move(dirt));

    BlockDef grass = BlockDef::Uniform("grass", 2);
    grass.faceTiles[static_cast<size_t>(BlockFace::PosY)] = 3;
    grass.faceTiles[static_cast<size_t>(BlockFace::NegY)] = 1;
    grass.hardness = 0.6f;
    grass.toolClass = ToolClass::Shovel;
    Grass = registry.Register(std::move(grass));

    BlockDef glowstone = BlockDef::Uniform("glowstone", 4);
    glowstone.emission = 15;
    glowstone.hardness = 0.3f; // vanilla glass material: no tool, no gating
    Glowstone = registry.Register(std::move(glowstone));

    BlockDef sand = BlockDef::Uniform("sand", 5);
    sand.gravity = true;
    sand.hardness = 0.5f;
    sand.toolClass = ToolClass::Shovel;
    Sand = registry.Register(std::move(sand));

    Log = registry.Register(LogDef("log", 6, 7));

    // M16: alpha-tested cutout leaves (vanilla "fancy graphics" look) —
    // every face against a non-opaque neighbor renders, including
    // leaf-on-leaf. lightOpacity 1 keeps a soft shadow under canopies.
    Leaves = registry.Register(LeavesDef("leaves", 8));

    BlockDef water = BlockDef::Uniform("water", 9);
    water.opaque = false; // light reaches the sea floor (attenuated); caves see through
    water.solid = false;  // no collision — swimming is the player's problem
    water.liquid = true;
    water.liquidLevel = 8;  // source
    water.lightOpacity = 3; // vanilla: skylight fades 3/block of depth
    Water = registry.Register(std::move(water));

    // Flow levels share the source's look and properties; the level only
    // drives the spread/recede simulation (and, later, render height).
    for (int level = 1; level <= 7; ++level) {
        BlockDef flow = BlockDef::Uniform("flowing_water_" + std::to_string(level), 9);
        flow.opaque = false;
        flow.solid = false;
        flow.liquid = true;
        flow.liquidLevel = static_cast<uint8_t>(level);
        flow.lightOpacity = 3;
        WaterFlows[static_cast<size_t>(level - 1)] = registry.Register(std::move(flow));
    }

    // M15 biomes (appended after the M13 flow ids — save blobs store
    // BlockIds): tiles 10 snow top, 11 snowed grass side.
    BlockDef snowyGrass = BlockDef::Uniform("snowy grass", 11);
    snowyGrass.faceTiles[static_cast<size_t>(BlockFace::PosY)] = 10;
    snowyGrass.faceTiles[static_cast<size_t>(BlockFace::NegY)] = 1;
    snowyGrass.hardness = 0.6f;
    snowyGrass.toolClass = ToolClass::Shovel;
    SnowyGrass = registry.Register(std::move(snowyGrass));

    // M16 plants (appended after SnowyGrass): cross-meshed, alpha-tested,
    // non-solid decoration. Tiles 12..15 — keep both atlas scripts in sync.
    TallGrass = registry.Register(Plant("tall grass", 12));
    Dandelion = registry.Register(Plant("dandelion", 13));
    Poppy = registry.Register(Plant("poppy", 14));
    DeadBush = registry.Register(Plant("dead bush", 15));

    // M16 tree species + cactus (tiles 16..23 — keep both atlas scripts
    // in sync). The cactus renders as a full opaque cube for now; its
    // import bakes the texture's transparent edge pixels opaque.
    BirchLog = registry.Register(LogDef("birch log", 16, 17));
    BirchLeaves = registry.Register(LeavesDef("birch leaves", 18));
    SpruceLog = registry.Register(LogDef("spruce log", 19, 20));
    SpruceLeaves = registry.Register(LeavesDef("spruce leaves", 21));
    BlockDef cactus = LogDef("cactus", 22, 23);
    cactus.hardness = 0.4f; // vanilla (LogDef defaulted it to wood's 2.0)
    cactus.toolClass = ToolClass::None; // vanilla cactus material: no tool
    Cactus = registry.Register(std::move(cactus));

    // M16: sandstone (tiles 24 side / 25 top / 26 bottom) — the vanilla
    // buffer under sand so cave ceilings in deserts don't hover.
    BlockDef sandstone = BlockDef::Uniform("sandstone", 24);
    sandstone.faceTiles[static_cast<size_t>(BlockFace::PosY)] = 25;
    sandstone.faceTiles[static_cast<size_t>(BlockFace::NegY)] = 26;
    sandstone.hardness = 0.8f;
    sandstone.toolClass = ToolClass::Pickaxe;
    sandstone.needsPickaxe = true;
    Sandstone = registry.Register(std::move(sandstone));

    // M16: bedrock world floor (tile 27) — solid at y0, ragged through
    // y4 (vanilla's rand.nextInt(5) rule); the break edit refuses it.
    BlockDef bedrock = BlockDef::Uniform("bedrock", 27);
    bedrock.unbreakable = true;
    Bedrock = registry.Register(std::move(bedrock));

    // M18: cobblestone (tile 28) — never generated, exists as stone's
    // mining drop. Crack overlay tiles follow at kFirstCrackTile (29..38).
    BlockDef cobble = BlockDef::Uniform("cobblestone", 28);
    cobble.hardness = 2.0f;
    cobble.toolClass = ToolClass::Pickaxe;
    cobble.needsPickaxe = true;
    Cobblestone = registry.Register(std::move(cobble));

    // M19: planks (tile 39) — crafted from any log, the wooden-tool
    // material; crafting table (tiles 40 top / 41 side / 42 front, planks
    // underneath) — RMB on it opens the 3x3 grid.
    BlockDef planks = BlockDef::Uniform("planks", 39);
    planks.hardness = 2.0f;
    planks.toolClass = ToolClass::Axe;
    Planks = registry.Register(std::move(planks));

    BlockDef craftingTable = BlockDef::Uniform("crafting table", 41);
    craftingTable.faceTiles[static_cast<size_t>(BlockFace::PosY)] = 40;
    craftingTable.faceTiles[static_cast<size_t>(BlockFace::NegY)] = 39;
    craftingTable.faceTiles[static_cast<size_t>(BlockFace::PosX)] = 42;
    craftingTable.hardness = 2.5f;
    craftingTable.toolClass = ToolClass::Axe;
    CraftingTable = registry.Register(std::move(craftingTable));

    // M21 ores (tiles 50/51, after the M19 item sprites at 43..49).
    // Vanilla hardness 3.0 for both. Coal's drop is the coal ITEM, whose
    // id doesn't exist yet — items::RegisterDefaults patches it in. Iron
    // ore drops itself (smelt it) and needs a stone-tier pickaxe.
    BlockDef coalOre = BlockDef::Uniform("coal ore", 50);
    coalOre.hardness = 3.0f;
    coalOre.toolClass = ToolClass::Pickaxe;
    coalOre.needsPickaxe = true;
    CoalOre = registry.Register(std::move(coalOre));

    BlockDef ironOre = BlockDef::Uniform("iron ore", 51);
    ironOre.hardness = 3.0f;
    ironOre.toolClass = ToolClass::Pickaxe;
    ironOre.needsPickaxe = true;
    ironOre.harvestLevel = 1; // vanilla: stone pickaxe or better
    IronOre = registry.Register(std::move(ironOre));

    // M21 furnace (tiles 52 front off / 53 front on / 54 side / 55 top):
    // front faces +X like the crafting table (no orientation data yet).
    // The lit variant is a separate id (swap on burn state) and emits
    // light 13 (vanilla 0.875 luminance); it drops the unlit furnace.
    const auto furnaceDef = [](std::string name, uint16_t front) {
        BlockDef def = BlockDef::Uniform(std::move(name), 54);
        def.faceTiles[static_cast<size_t>(BlockFace::PosY)] = 55;
        def.faceTiles[static_cast<size_t>(BlockFace::NegY)] = 55;
        def.faceTiles[static_cast<size_t>(BlockFace::PosX)] = front;
        def.hardness = 3.5f;
        def.toolClass = ToolClass::Pickaxe;
        def.needsPickaxe = true;
        return def;
    };
    Furnace = registry.Register(furnaceDef("furnace", 52));
    BlockDef litFurnace = furnaceDef("lit furnace", 53);
    litFurnace.emission = 13;
    LitFurnace = registry.Register(std::move(litFurnace));

    // M21 glass (tile 56): smelted from sand; alpha-tested cutout cube
    // like leaves (so glass-on-glass interior faces show — vanilla culls
    // them, close enough). Vanilla: drops nothing.
    BlockDef glass = BlockDef::Uniform("glass", 56);
    glass.opaque = false;
    glass.cutout = true;
    glass.hardness = 0.3f;
    glass.drop = blocks::Air;
    Glass = registry.Register(std::move(glass));

    // M18 drop table (vanilla-ish; cross-references resolve here, after
    // every id exists). Leaves/tall grass/dead bush already drop nothing.
    registry.EditDef(Stone).drop = Cobblestone;
    registry.EditDef(Grass).drop = Dirt;
    registry.EditDef(SnowyGrass).drop = Dirt;
    registry.EditDef(TallGrass).drop = Air; // no seeds/shears yet
    registry.EditDef(DeadBush).drop = Air;
    registry.EditDef(LitFurnace).drop = Furnace;

    GAME_INFO("Registered {} block types", registry.Count());
}

} // namespace blocks

} // namespace vc
