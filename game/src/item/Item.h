#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "world/Block.h"

namespace vc {

// Unified item id space (M19): ids below kFirstItemId ARE BlockIds (every
// placeable block is an item); ids from kFirstItemId up index the
// ItemRegistry (sticks, tools — things that exist only in inventories and
// as drops). Both halves are persisted in save manifests, so registration
// order may only APPEND, like blocks.
using ItemId = uint16_t;
inline constexpr ItemId kFirstItemId = 1024;

// M33: the four worn-armor slots. Indices 0..3 double as the armor array
// index in Inventory and the display order top-to-bottom in the doll UI.
enum class ArmorSlot { Head = 0, Chest = 1, Legs = 2, Feet = 3 };
inline constexpr int kArmorSlots = 4;

struct ItemDef {
    std::string name;
    uint16_t tile = 0; // atlas layer of the 2D sprite (UI icon + world drop)
    ToolClass tool = ToolClass::None;
    float efficiency = 1.0f; // dig-speed multiplier on matching blocks
    int maxDamage = 0;       // tool durability in uses; 0 = not damageable
    int maxStack = 64;       // tools stack to 1
    int tier = 0; // pickaxe harvest tier vs BlockDef::harvestLevel (M21):
                  // wood 0, stone 1, iron 2
    // M33 armor: `armor` flags an equippable piece; the rest are the vanilla
    // 1.12 ItemArmor.ArmorMaterial values (defense points + toughness feed
    // CombatRules.getDamageAfterAbsorb). armorTexture is the material key
    // ("iron", "diamond", ...) used to pick the worn-layer model texture.
    bool armor = false;
    ArmorSlot armorSlot = ArmorSlot::Head;
    int defensePoints = 0;
    float armorToughness = 0.0f;
    std::string armorTexture;
};

class ItemRegistry {
public:
    static ItemRegistry& Get();

    ItemId Register(ItemDef def); // returns kFirstItemId + index
    // Null for blocks and out-of-range ids — the caller's signal that the
    // id is (or should be) a BlockId.
    const ItemDef* Find(ItemId id) const;
    size_t Count() const { return m_defs.size(); }

private:
    std::vector<ItemDef> m_defs;
};

// Helpers spanning both halves of the id space.
inline bool IsBlockItem(ItemId id) { return id < kFirstItemId; }
// True when the id draws as a flat sprite in 3D (held + dropped):
// registry items always, plus sprite-like blocks (plants, torches) —
// a mini cube of their texture reads wrong.
bool RenderAsSprite(ItemId id);
bool ItemExists(ItemId id); // save-load validation
const std::string& ItemName(ItemId id);
uint16_t ItemIconTile(ItemId id); // sprite tile, or the block's side face
int ItemMaxStack(ItemId id);

// M33 armor queries (all false/zero for non-armor ids).
bool IsArmor(ItemId id);
ArmorSlot ArmorSlotOf(ItemId id); // meaningful only when IsArmor(id)
int ArmorDefense(ItemId id);
float ArmorToughness(ItemId id);
const std::string& ArmorTexture(ItemId id); // material key, e.g. "iron"

// Atlas tiles for the empty-armor-slot placeholder sprites, indexed by
// ArmorSlot (Head/Chest/Legs/Feet). Drawn by InventoryScreen; not items.
// Must match both atlas scripts (tiles 91..94, after the M33 armor icons).
inline constexpr uint16_t kEmptyArmorSlotTile[kArmorSlots] = {91, 92, 93, 94};

namespace items {

extern ItemId Stick;
extern ItemId WoodPickaxe;
extern ItemId WoodAxe;
extern ItemId WoodShovel;
extern ItemId StonePickaxe;
extern ItemId StoneAxe;
extern ItemId StoneShovel;
// M21: coal (fuel, from coal ore), iron ingot (smelted from iron ore),
// and the iron tool tier.
extern ItemId Coal;
extern ItemId IronIngot;
extern ItemId IronPickaxe;
extern ItemId IronAxe;
extern ItemId IronShovel;
// M26 follow-up: buckets. Empty bucket (3 iron, stacks to 16) picks up a
// liquid SOURCE on use and becomes the matching filled bucket (stack 1);
// using a filled bucket places that liquid source back and empties it.
extern ItemId Bucket;
extern ItemId WaterBucket;
extern ItemId LavaBucket;
// M32 mob drops (sprite-only for now; eating/food value is backlog).
extern ItemId RawPorkchop; // pig
extern ItemId RottenFlesh;  // zombie
// M34 passive-mob drops (sprite-only; food value is backlog). Egg is a plain
// sprite for now — throwing it / spawning chicks is deferred. Shears is a
// damageable tool used to shear sheep (no dig-speed bonus yet).
extern ItemId RawBeef;     // cow
extern ItemId LeatherItem; // cow (name avoids the ArmorMaterial::Leather enum)
extern ItemId RawMutton;   // sheep
extern ItemId RawChicken;  // chicken
extern ItemId Feather;     // chicken
extern ItemId Egg;         // chicken lays these
extern ItemId Shears;      // shear a sheep for wool

// M33 armor: the 20 pieces register contiguously, material-major then slot,
// starting at FirstArmor (leather helmet). ArmorPiece() addresses one by
// material + slot rather than 20 named externs.
enum ArmorMaterial { Leather = 0, Chainmail = 1, Iron = 2, Gold = 3, Diamond = 4 };
extern ItemId FirstArmor;
inline ItemId ArmorPiece(ArmorMaterial material, ArmorSlot slot) {
    return static_cast<ItemId>(FirstArmor + material * kArmorSlots + static_cast<int>(slot));
}

// The liquid SOURCE block a filled bucket places (Air for the empty bucket
// or any non-bucket item) — lets the bucket use code stay data-driven.
BlockId BucketLiquid(ItemId id);
// The filled-bucket item for a given liquid source block (0 if that block
// isn't a bucketable source).
ItemId FilledBucketFor(BlockId source);

// Call after blocks::RegisterDefaults() (item sprites follow the block
// tiles in the atlas).
void RegisterDefaults();

} // namespace items

} // namespace vc
