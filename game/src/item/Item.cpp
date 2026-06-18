#include "item/Item.h"

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

bool RenderAsSprite(ItemId id) {
    if (!IsBlockItem(id)) {
        return true;
    }
    const BlockDef& def = BlockRegistry::Get().Def(id);
    return def.cross || def.torch;
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

bool IsArmor(ItemId id) {
    const ItemDef* item = ItemRegistry::Get().Find(id);
    return item != nullptr && item->armor;
}

ArmorSlot ArmorSlotOf(ItemId id) {
    if (const ItemDef* item = ItemRegistry::Get().Find(id)) {
        return item->armorSlot;
    }
    return ArmorSlot::Head;
}

int ArmorDefense(ItemId id) {
    const ItemDef* item = ItemRegistry::Get().Find(id);
    return (item != nullptr && item->armor) ? item->defensePoints : 0;
}

float ArmorToughness(ItemId id) {
    const ItemDef* item = ItemRegistry::Get().Find(id);
    return (item != nullptr && item->armor) ? item->armorToughness : 0.0f;
}

const std::string& ArmorTexture(ItemId id) {
    static const std::string kEmpty;
    const ItemDef* item = ItemRegistry::Get().Find(id);
    return (item != nullptr && item->armor) ? item->armorTexture : kEmpty;
}

namespace items {

BlockId BucketLiquid(ItemId id) {
    if (id == WaterBucket) {
        return blocks::Water;
    }
    if (id == LavaBucket) {
        return blocks::Lava;
    }
    return blocks::Air;
}

ItemId FilledBucketFor(BlockId source) {
    if (source == blocks::Water) {
        return WaterBucket;
    }
    if (source == blocks::Lava) {
        return LavaBucket;
    }
    return 0;
}

ItemId Stick = 0;
ItemId WoodPickaxe = 0;
ItemId WoodAxe = 0;
ItemId WoodShovel = 0;
ItemId StonePickaxe = 0;
ItemId StoneAxe = 0;
ItemId StoneShovel = 0;
ItemId Coal = 0;
ItemId IronIngot = 0;
ItemId IronPickaxe = 0;
ItemId IronAxe = 0;
ItemId IronShovel = 0;
ItemId Bucket = 0;
ItemId WaterBucket = 0;
ItemId LavaBucket = 0;
ItemId RawPorkchop = 0;
ItemId RottenFlesh = 0;
ItemId RawBeef = 0;
ItemId LeatherItem = 0;
ItemId RawMutton = 0;
ItemId RawChicken = 0;
ItemId Feather = 0;
ItemId Egg = 0;
ItemId Shears = 0;
ItemId Gunpowder = 0;
ItemId FlintAndSteel = 0;
ItemId FirstArmor = 0;

namespace {

// Vanilla ToolMaterial: WOOD uses 59 / efficiency 2 / tier 0, STONE
// 131 / 4 / 1, IRON 250 / 6 / 2.
ItemDef Tool(std::string name, uint16_t tile, ToolClass tool, float efficiency, int uses,
             int tier) {
    ItemDef def;
    def.name = std::move(name);
    def.tile = tile;
    def.tool = tool;
    def.efficiency = efficiency;
    def.maxDamage = uses;
    def.maxStack = 1;
    def.tier = tier;
    return def;
}

} // namespace

void RegisterDefaults() {
    auto& registry = ItemRegistry::Get();
    if (Stick != 0) {
        return; // already registered
    }

    // Sprite tiles 43..49 follow the M19 block tiles, 57..61 the M21
    // furnace/ore tiles — keep BOTH scripts/gen_textures.py and
    // scripts/import_mc_assets.py in sync.
    ItemDef stick;
    stick.name = "stick";
    stick.tile = 43;
    Stick = registry.Register(std::move(stick));

    WoodPickaxe = registry.Register(Tool("wooden pickaxe", 44, ToolClass::Pickaxe, 2.0f, 59, 0));
    WoodAxe = registry.Register(Tool("wooden axe", 45, ToolClass::Axe, 2.0f, 59, 0));
    WoodShovel = registry.Register(Tool("wooden shovel", 46, ToolClass::Shovel, 2.0f, 59, 0));
    StonePickaxe =
        registry.Register(Tool("stone pickaxe", 47, ToolClass::Pickaxe, 4.0f, 131, 1));
    StoneAxe = registry.Register(Tool("stone axe", 48, ToolClass::Axe, 4.0f, 131, 1));
    StoneShovel = registry.Register(Tool("stone shovel", 49, ToolClass::Shovel, 4.0f, 131, 1));

    // M21 (appended — item ids persist in save manifests, like blocks).
    ItemDef coal;
    coal.name = "coal";
    coal.tile = 57;
    Coal = registry.Register(std::move(coal));

    ItemDef ingot;
    ingot.name = "iron ingot";
    ingot.tile = 58;
    IronIngot = registry.Register(std::move(ingot));

    IronPickaxe = registry.Register(Tool("iron pickaxe", 59, ToolClass::Pickaxe, 6.0f, 250, 2));
    IronAxe = registry.Register(Tool("iron axe", 60, ToolClass::Axe, 6.0f, 250, 2));
    IronShovel = registry.Register(Tool("iron shovel", 61, ToolClass::Shovel, 6.0f, 250, 2));

    // M26 follow-up: buckets (sprite tiles 66/67/68 — appended after the
    // M26 lava/obsidian block tiles; keep BOTH atlas scripts in sync). The
    // empty bucket stacks to 16 (vanilla); filled buckets to 1.
    ItemDef bucket;
    bucket.name = "bucket";
    bucket.tile = 66;
    bucket.maxStack = 16;
    Bucket = registry.Register(std::move(bucket));

    ItemDef waterBucket;
    waterBucket.name = "water bucket";
    waterBucket.tile = 67;
    waterBucket.maxStack = 1;
    WaterBucket = registry.Register(std::move(waterBucket));

    ItemDef lavaBucket;
    lavaBucket.name = "lava bucket";
    lavaBucket.tile = 68;
    lavaBucket.maxStack = 1;
    LavaBucket = registry.Register(std::move(lavaBucket));

    // M32 mob drops (sprite tiles 69/70 — appended after the M26 bucket tiles;
    // keep BOTH atlas scripts in sync). Sprite-only: no food value yet.
    ItemDef porkchop;
    porkchop.name = "raw porkchop";
    porkchop.tile = 69;
    RawPorkchop = registry.Register(std::move(porkchop));

    ItemDef flesh;
    flesh.name = "rotten flesh";
    flesh.tile = 70;
    RottenFlesh = registry.Register(std::move(flesh));

    // M33 armor: 5 materials x 4 slots = 20 pieces (sprite tiles 71..90,
    // material-major, slot order helmet/chestplate/leggings/boots — matches
    // ArmorSlot Head/Chest/Legs/Feet = 0..3). Values are vanilla 1.12
    // ItemArmor.ArmorMaterial: durability = MAX_DAMAGE_ARRAY[slot] * the
    // material factor; defense points + toughness straight from the enum
    // (only diamond has toughness 2.0). Keep BOTH atlas scripts in sync
    // (gen_textures.py placeholders + import_mc_assets.py real icons).
    struct ArmorMat {
        const char* key;     // layer-texture material key (file name stem)
        const char* display; // item name prefix
        int factor;          // durability multiplier
        float toughness;
    };
    static constexpr ArmorMat kArmorMats[] = {
        {"leather", "leather", 5, 0.0f},   {"chainmail", "chainmail", 15, 0.0f},
        {"iron", "iron", 15, 0.0f},        {"gold", "golden", 7, 0.0f},
        {"diamond", "diamond", 33, 2.0f},
    };
    static constexpr int kSlotMaxDamage[kArmorSlots] = {11, 16, 15, 13}; // Head,Chest,Legs,Feet
    static constexpr const char* kSlotName[kArmorSlots] = {"helmet", "chestplate", "leggings",
                                                           "boots"};
    // Defense points per material per slot, in Head/Chest/Legs/Feet order
    // (vanilla's damageReductionAmountArray reindexed from feet-first).
    static constexpr int kArmorDefense[5][kArmorSlots] = {
        {1, 3, 2, 1}, {2, 5, 4, 1}, {2, 6, 5, 2}, {2, 5, 3, 1}, {3, 8, 6, 3},
    };
    uint16_t armorTile = 71;
    for (int m = 0; m < 5; ++m) {
        for (int s = 0; s < kArmorSlots; ++s) {
            ItemDef a;
            a.name = std::string(kArmorMats[m].display) + " " + kSlotName[s];
            a.tile = armorTile++;
            a.maxStack = 1;
            a.maxDamage = kSlotMaxDamage[s] * kArmorMats[m].factor;
            a.armor = true;
            a.armorSlot = static_cast<ArmorSlot>(s);
            a.defensePoints = kArmorDefense[m][s];
            a.armorToughness = kArmorMats[m].toughness;
            a.armorTexture = kArmorMats[m].key;
            const ItemId id = registry.Register(std::move(a));
            if (m == 0 && s == 0) {
                FirstArmor = id;
            }
        }
    }

    // M34 passive-mob drops + shears (sprite tiles 96..102, after the wool
    // BLOCK tile 95 — keep BOTH atlas scripts in sync). All plain sprites
    // except shears, a 238-use damageable tool (vanilla) with no dig bonus yet.
    const auto sprite = [&](const char* name, uint16_t tile) {
        ItemDef d;
        d.name = name;
        d.tile = tile;
        return registry.Register(std::move(d));
    };
    RawBeef = sprite("raw beef", 96);
    LeatherItem = sprite("leather", 97);
    RawMutton = sprite("raw mutton", 98);
    RawChicken = sprite("raw chicken", 99);
    Feather = sprite("feather", 100);
    Egg = sprite("egg", 101);
    ItemDef shears;
    shears.name = "shears";
    shears.tile = 102;
    shears.maxStack = 1;
    shears.maxDamage = 238; // vanilla Items.SHEARS
    Shears = registry.Register(std::move(shears));

    // M35 explosives (sprite tiles 106 gunpowder / 107 flint & steel — after
    // the M35 TNT block tiles 103..105; keep BOTH atlas scripts in sync).
    // Gunpowder is a plain sprite; flint & steel is a 64-use damageable igniter
    // (vanilla durability 65, no dig bonus — mirrors shears).
    Gunpowder = sprite("gunpowder", 106);
    ItemDef flintAndSteel;
    flintAndSteel.name = "flint and steel";
    flintAndSteel.tile = 107;
    flintAndSteel.maxStack = 1;
    flintAndSteel.maxDamage = 64; // vanilla Items.FLINT_AND_STEEL (65 uses)
    FlintAndSteel = registry.Register(std::move(flintAndSteel));

    // Coal ore's drop is the coal item — its id only exists now, after
    // item registration (same late-patch pattern as stone -> cobblestone).
    BlockRegistry::Get().EditDef(blocks::CoalOre).drop = Coal;

    GAME_INFO("Registered {} item types (ids from {})", registry.Count(), kFirstItemId);
}

} // namespace items

} // namespace vc
