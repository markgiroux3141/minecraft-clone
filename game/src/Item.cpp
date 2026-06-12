#include "Item.h"

#include "vox/core/Log.h"

namespace vc {

ItemRegistry& ItemRegistry::Get() {
    static ItemRegistry instance;
    return instance;
}

ItemId ItemRegistry::Register(ItemDef def) {
    const auto id = static_cast<ItemId>(kFirstItemId + m_defs.size());
    m_defs.push_back(std::move(def));
    return id;
}

const ItemDef* ItemRegistry::Find(ItemId id) const {
    if (id < kFirstItemId || id - kFirstItemId >= m_defs.size()) {
        return nullptr;
    }
    return &m_defs[id - kFirstItemId];
}

bool ItemExists(ItemId id) {
    if (IsBlockItem(id)) {
        return id < BlockRegistry::Get().Count();
    }
    return ItemRegistry::Get().Find(id) != nullptr;
}

const std::string& ItemName(ItemId id) {
    if (const ItemDef* item = ItemRegistry::Get().Find(id)) {
        return item->name;
    }
    return BlockRegistry::Get().Def(IsBlockItem(id) ? id : 0).name;
}

uint16_t ItemIconTile(ItemId id) {
    if (const ItemDef* item = ItemRegistry::Get().Find(id)) {
        return item->tile;
    }
    // Side face: for grass that's the fringe tile, more recognizable than
    // plain green from the top.
    return BlockRegistry::Get().Def(IsBlockItem(id) ? id : 0)
        .faceTiles[static_cast<size_t>(BlockFace::PosX)];
}

int ItemMaxStack(ItemId id) {
    if (const ItemDef* item = ItemRegistry::Get().Find(id)) {
        return item->maxStack;
    }
    return 64;
}

namespace items {

ItemId Stick = 0;
ItemId WoodPickaxe = 0;
ItemId WoodAxe = 0;
ItemId WoodShovel = 0;
ItemId StonePickaxe = 0;
ItemId StoneAxe = 0;
ItemId StoneShovel = 0;

namespace {

// Vanilla ToolMaterial: WOOD uses 59 / efficiency 2, STONE 131 / 4.
ItemDef Tool(std::string name, uint16_t tile, ToolClass tool, float efficiency, int uses) {
    ItemDef def;
    def.name = std::move(name);
    def.tile = tile;
    def.tool = tool;
    def.efficiency = efficiency;
    def.maxDamage = uses;
    def.maxStack = 1;
    return def;
}

} // namespace

void RegisterDefaults() {
    auto& registry = ItemRegistry::Get();
    if (Stick != 0) {
        return; // already registered
    }

    // Sprite tiles 43..49 follow the M19 block tiles — keep BOTH
    // scripts/gen_textures.py and scripts/import_mc_assets.py in sync.
    ItemDef stick;
    stick.name = "stick";
    stick.tile = 43;
    Stick = registry.Register(std::move(stick));

    WoodPickaxe = registry.Register(Tool("wooden pickaxe", 44, ToolClass::Pickaxe, 2.0f, 59));
    WoodAxe = registry.Register(Tool("wooden axe", 45, ToolClass::Axe, 2.0f, 59));
    WoodShovel = registry.Register(Tool("wooden shovel", 46, ToolClass::Shovel, 2.0f, 59));
    StonePickaxe = registry.Register(Tool("stone pickaxe", 47, ToolClass::Pickaxe, 4.0f, 131));
    StoneAxe = registry.Register(Tool("stone axe", 48, ToolClass::Axe, 4.0f, 131));
    StoneShovel = registry.Register(Tool("stone shovel", 49, ToolClass::Shovel, 4.0f, 131));

    GAME_INFO("Registered {} item types (ids from {})", registry.Count(), kFirstItemId);
}

} // namespace items

} // namespace vc
