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
BlockId Torch = 0;
BlockId Lava = 0;
std::array<BlockId, 7> LavaFlows{};
BlockId Obsidian = 0;

namespace {

BlockDef Plant(std::string name, uint16_t tile) {
    BlockDef def = BlockDef::Uniform(std::move(name), tile);
    def.opaque = false;
    def.solid = false; // walk through; raycast still targets it (cross)
    def.cross = true;
    def.replaceable = true;
    def.hardness = 0.0f; // vanilla: plants break instantly
    def.soundType = SoundType::Grass;
    return def;
}

BlockDef LogDef(std::string name, uint16_t sideTile, uint16_t topTile) {
    BlockDef def = BlockDef::Uniform(std::move(name), sideTile);
    def.faceTiles[static_cast<size_t>(BlockFace::PosY)] = topTile;
    def.faceTiles[static_cast<size_t>(BlockFace::NegY)] = topTile;
    def.hardness = 2.0f; // vanilla BlockLog
    def.toolClass = ToolClass::Axe;
    def.soundType = SoundType::Wood;
    return def;
}

BlockDef LeavesDef(std::string name, uint16_t tile) {
    BlockDef def = BlockDef::Uniform(std::move(name), tile);
    def.opaque = false;
    def.cutout = true;
    def.lightOpacity = 1;
    def.hardness = 0.2f;     // vanilla BlockLeaves
    def.drop = blocks::Air;  // nothing without shears/saplings (M18 table)
    def.soundType = SoundType::Grass;
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
    dirt.soundType = SoundType::Gravel; // vanilla soft "ground" dig
    Dirt = registry.Register(std::move(dirt));

    BlockDef grass = BlockDef::Uniform("grass", 2);
    grass.faceTiles[static_cast<size_t>(BlockFace::PosY)] = 3;
    grass.faceTiles[static_cast<size_t>(BlockFace::NegY)] = 1;
    grass.hardness = 0.6f;
    grass.toolClass = ToolClass::Shovel;
    grass.soundType = SoundType::Grass;
    Grass = registry.Register(std::move(grass));

    BlockDef glowstone = BlockDef::Uniform("glowstone", 4);
    glowstone.emission = 15;
    glowstone.hardness = 0.3f; // vanilla glass material: no tool, no gating
    Glowstone = registry.Register(std::move(glowstone));

    BlockDef sand = BlockDef::Uniform("sand", 5);
    sand.gravity = true;
    sand.hardness = 0.5f;
    sand.toolClass = ToolClass::Shovel;
    sand.soundType = SoundType::Sand;
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
    water.soundType = SoundType::None; // splash handled separately, no dig/step
    Water = registry.Register(std::move(water));
    registry.EditDef(Water).liquidSource = Water; // flow engine: water family tag

    // Flow levels share the source's look and properties; the level only
    // drives the spread/recede simulation (and, later, render height).
    for (int level = 1; level <= 7; ++level) {
        BlockDef flow = BlockDef::Uniform("flowing_water_" + std::to_string(level), 9);
        flow.opaque = false;
        flow.solid = false;
        flow.liquid = true;
        flow.liquidLevel = static_cast<uint8_t>(level);
        flow.liquidSource = Water;
        flow.lightOpacity = 3;
        flow.soundType = SoundType::None;
        WaterFlows[static_cast<size_t>(level - 1)] = registry.Register(std::move(flow));
    }

    // M15 biomes (appended after the M13 flow ids — save blobs store
    // BlockIds): tiles 10 snow top, 11 snowed grass side.
    BlockDef snowyGrass = BlockDef::Uniform("snowy grass", 11);
    snowyGrass.faceTiles[static_cast<size_t>(BlockFace::PosY)] = 10;
    snowyGrass.faceTiles[static_cast<size_t>(BlockFace::NegY)] = 1;
    snowyGrass.hardness = 0.6f;
    snowyGrass.toolClass = ToolClass::Shovel;
    snowyGrass.soundType = SoundType::Snow;
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
    cactus.soundType = SoundType::Cloth; // vanilla cactus material (LogDef set Wood)
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
    planks.soundType = SoundType::Wood;
    Planks = registry.Register(std::move(planks));

    BlockDef craftingTable = BlockDef::Uniform("crafting table", 41);
    craftingTable.faceTiles[static_cast<size_t>(BlockFace::PosY)] = 40;
    craftingTable.faceTiles[static_cast<size_t>(BlockFace::NegY)] = 39;
    craftingTable.faceTiles[static_cast<size_t>(BlockFace::PosX)] = 42;
    craftingTable.horizontalFacing = true; // M24: front points at the placer
    craftingTable.hardness = 2.5f;
    craftingTable.toolClass = ToolClass::Axe;
    craftingTable.soundType = SoundType::Wood;
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
    // front (PosX = canonical) is reoriented per-cell by M24 facing meta to
    // point at the placer; the side tile (54) fills the other horizontals.
    // The lit variant is a separate id (swap on burn state) and emits
    // light 13 (vanilla 0.875 luminance); it drops the unlit furnace.
    const auto furnaceDef = [](std::string name, uint16_t front) {
        BlockDef def = BlockDef::Uniform(std::move(name), 54);
        def.faceTiles[static_cast<size_t>(BlockFace::PosY)] = 55;
        def.faceTiles[static_cast<size_t>(BlockFace::NegY)] = 55;
        def.faceTiles[static_cast<size_t>(BlockFace::PosX)] = front;
        def.horizontalFacing = true; // M24: front points at the placer
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
    glass.soundType = SoundType::Glass; // break = random/glass; dig/step fall back to stone
    Glass = registry.Register(std::move(glass));

    // M21 follow-up: torch (tile 62, after the M21 item sprites at
    // 57..61). Floor-standing only; pops without solid ground below.
    // Replaceable like plants — flowing water washes it away (vanilla)
    // and its drop pops via CrushDrops. Instant break, light 14.
    BlockDef torch = BlockDef::Uniform("torch", 62);
    torch.opaque = false;
    torch.solid = false;
    torch.torch = true;
    torch.replaceable = true;
    torch.hardness = 0.0f;
    torch.emission = 14;
    torch.soundType = SoundType::Wood;
    // Geometry: vanilla's template_torch. Two thin full-height slabs cross
    // at the cell center, each showing only its outward faces — the post
    // pixels live in the texture's middle columns, the alpha test trims the
    // rest, and from any angle one X plane + one Z plane read as a 3D post.
    // A small top cap sits at the post's flame height (y = 10/16, exact now
    // that the model stream stores float positions — the old packed format
    // could only quantise it to ninths and had to bias it low to avoid a
    // gap). The cap samples the dedicated opaque flame-core sprite (tile
    // 63); the planes sample the torch tile (62).
    constexpr float kPlaneSide = 16.0f; // full-cell extent of the side planes
    {
        ModelBox xPlanes; // the two planes perpendicular to X (faces -X/+X)
        xPlanes.from = {7.0f, 0.0f, 0.0f};
        xPlanes.to = {9.0f, kPlaneSide, kPlaneSide};
        xPlanes.faces[static_cast<size_t>(BlockFace::NegX)] = {true, 62, {0, 0, 16, 16}};
        xPlanes.faces[static_cast<size_t>(BlockFace::PosX)] = {true, 62, {0, 0, 16, 16}};

        ModelBox zPlanes; // the two planes perpendicular to Z (faces -Z/+Z)
        zPlanes.from = {0.0f, 0.0f, 7.0f};
        zPlanes.to = {kPlaneSide, kPlaneSide, 9.0f};
        zPlanes.faces[static_cast<size_t>(BlockFace::NegZ)] = {true, 62, {0, 0, 16, 16}};
        zPlanes.faces[static_cast<size_t>(BlockFace::PosZ)] = {true, 62, {0, 0, 16, 16}};

        ModelBox cap; // top of the central 2x2 post, at the flame height
        cap.from = {7.0f, 0.0f, 7.0f};
        cap.to = {9.0f, 10.0f, 9.0f};
        cap.faces[static_cast<size_t>(BlockFace::PosY)] = {true, 63, {0, 0, 16, 16}};

        torch.model = {xPlanes, zPlanes, cap};
    }
    Torch = registry.Register(std::move(torch));

    // M26 lava (tile 64 still). Like water it meshes in the liquid pass
    // (partial-height surface) but reads OPAQUE (texture alpha 255) and
    // emits light 15. lightOpacity 15 means sky/block light can't pass
    // THROUGH lava (vanilla: lava is a full light blocker) — the emission
    // still floods out of the cell, so it lights deep cave floors. It's a
    // SOURCE here (worldgen pools it statically); flows are appended next.
    const auto lavaDef = [](std::string name, uint8_t level) {
        BlockDef def = BlockDef::Uniform(std::move(name), 64);
        def.opaque = false;
        def.solid = false;
        def.liquid = true;
        def.liquidOpaque = true; // opaque pass — lava isn't see-through
        def.liquidLevel = level;
        def.emission = 15;
        def.lightOpacity = 15;
        def.soundType = SoundType::None;
        return def;
    };
    Lava = registry.Register(lavaDef("lava", 8));
    registry.EditDef(Lava).liquidSource = Lava; // flow engine: lava family tag
    for (int level = 1; level <= 7; ++level) {
        BlockDef flow = lavaDef("flowing_lava_" + std::to_string(level),
                                static_cast<uint8_t>(level));
        flow.liquidSource = Lava;
        LavaFlows[static_cast<size_t>(level - 1)] = registry.Register(std::move(flow));
    }

    // M26 obsidian (tile 65): forms where a lava SOURCE meets water. Vanilla
    // hardness 50 + diamond-pick gating; with no diamond tier yet it harvests
    // with the iron pickaxe (harvestLevel 2). Drops itself.
    BlockDef obsidian = BlockDef::Uniform("obsidian", 65);
    obsidian.hardness = 50.0f;
    obsidian.toolClass = ToolClass::Pickaxe;
    obsidian.needsPickaxe = true;
    obsidian.harvestLevel = 2;
    Obsidian = registry.Register(std::move(obsidian));

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
