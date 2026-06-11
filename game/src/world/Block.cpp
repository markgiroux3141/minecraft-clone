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

    Sand = registry.Register(BlockDef::Uniform("sand", 5));

    BlockDef log = BlockDef::Uniform("log", 6);
    log.faceTiles[static_cast<size_t>(BlockFace::PosY)] = 7;
    log.faceTiles[static_cast<size_t>(BlockFace::NegY)] = 7;
    Log = registry.Register(std::move(log));

    // Opaque for now; cutout leaves come with the M11 transparency pass.
    Leaves = registry.Register(BlockDef::Uniform("leaves", 8));

    BlockDef water = BlockDef::Uniform("water", 9);
    water.opaque = false; // sky light reaches the sea floor; caves see through it
    water.solid = false;  // no collision — swimming is the player's problem
    water.liquid = true;
    Water = registry.Register(std::move(water));

    GAME_INFO("Registered {} block types", registry.Count());
}

} // namespace blocks

} // namespace vc
