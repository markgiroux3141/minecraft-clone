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

namespace {

BlockDef Plant(std::string name, uint16_t tile) {
    BlockDef def = BlockDef::Uniform(std::move(name), tile);
    def.opaque = false;
    def.solid = false; // walk through; raycast still targets it (cross)
    def.cross = true;
    def.replaceable = true;
    return def;
}

BlockDef LogDef(std::string name, uint16_t sideTile, uint16_t topTile) {
    BlockDef def = BlockDef::Uniform(std::move(name), sideTile);
    def.faceTiles[static_cast<size_t>(BlockFace::PosY)] = topTile;
    def.faceTiles[static_cast<size_t>(BlockFace::NegY)] = topTile;
    return def;
}

BlockDef LeavesDef(std::string name, uint16_t tile) {
    BlockDef def = BlockDef::Uniform(std::move(name), tile);
    def.opaque = false;
    def.cutout = true;
    def.lightOpacity = 1;
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
    Stone = registry.Register(BlockDef::Uniform("stone", 0));
    Dirt = registry.Register(BlockDef::Uniform("dirt", 1));

    BlockDef grass = BlockDef::Uniform("grass", 2);
    grass.faceTiles[static_cast<size_t>(BlockFace::PosY)] = 3;
    grass.faceTiles[static_cast<size_t>(BlockFace::NegY)] = 1;
    Grass = registry.Register(std::move(grass));

    BlockDef glowstone = BlockDef::Uniform("glowstone", 4);
    glowstone.emission = 15;
    Glowstone = registry.Register(std::move(glowstone));

    BlockDef sand = BlockDef::Uniform("sand", 5);
    sand.gravity = true;
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
    Cactus = registry.Register(LogDef("cactus", 22, 23));

    // M16: sandstone (tiles 24 side / 25 top / 26 bottom) — the vanilla
    // buffer under sand so cave ceilings in deserts don't hover.
    BlockDef sandstone = BlockDef::Uniform("sandstone", 24);
    sandstone.faceTiles[static_cast<size_t>(BlockFace::PosY)] = 25;
    sandstone.faceTiles[static_cast<size_t>(BlockFace::NegY)] = 26;
    Sandstone = registry.Register(std::move(sandstone));

    // M16: bedrock world floor (tile 27) — solid at y0, ragged through
    // y4 (vanilla's rand.nextInt(5) rule); the break edit refuses it.
    BlockDef bedrock = BlockDef::Uniform("bedrock", 27);
    bedrock.unbreakable = true;
    Bedrock = registry.Register(std::move(bedrock));

    GAME_INFO("Registered {} block types", registry.Count());
}

} // namespace blocks

} // namespace vc
