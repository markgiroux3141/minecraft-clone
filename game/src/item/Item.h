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
    // M37 food (vanilla 1.12 ItemFood): `food` flags an edible item; foodPoints
    // refills hunger (2 = one drumstick), saturationModifier feeds the hidden
    // saturation buffer (vanilla saturation gain = foodPoints * modifier * 2).
    // alwaysEdible lets the item be eaten at full hunger (rotten flesh, the
    // golden apple) — every other food is gated on hunger < 20.
    bool food = false;
    int foodPoints = 0;
    float saturationModifier = 0.0f;
    bool alwaysEdible = false;
    // RS1: a non-block item that, on RMB against a surface, places a DIFFERENT
    // block than itself (redstone dust -> the wire block; like the water bucket
    // placing a water source). 0 = this item places nothing. The block's own
    // placement rules (e.g. wire needs solid ground below) still apply.
    BlockId placesBlock = 0;
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

// M37 food queries (all false/zero for non-food ids). FoodSaturation applies
// vanilla's foodPoints * modifier * 2; AlwaysEdible gates eating at full hunger.
bool IsFood(ItemId id);
int FoodPoints(ItemId id);
float FoodSaturation(ItemId id);
bool AlwaysEdible(ItemId id);

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
// M35 explosives: gunpowder (creeper drop + TNT ingredient) and flint & steel
// (a damageable igniter — RMB a TNT block to prime it, or a creeper to detonate
// it). Flint & steel substitutes coal for vanilla's flint (no gravel/flint yet).
extern ItemId Gunpowder;
extern ItemId FlintAndSteel;
// M36 projectiles: the bow (a 384-use launcher; RMB-hold to draw), arrow (ammo +
// skeleton drop), and bone (skeleton drop). The bow draws ammo from any Arrow
// stack in the inventory and fires a player-owned Arrow entity.
extern ItemId Bow;
extern ItemId Arrow;
extern ItemId Bone;
// M37 cooked foods: smelt the raw meat drops in a furnace. Better food values
// than the raw versions (vanilla 1.12 ItemFood).
extern ItemId CookedPorkchop;
extern ItemId CookedBeef; // "steak"
extern ItemId CookedMutton;
extern ItemId CookedChicken;
// M38 breeding items (plain sprites — no farming source yet, grab them from the
// creative palette). RMB-feeding two adult animals their breed item puts them in
// love mode and spawns a baby (see IsBreedingFood in Mob.cpp): wheat breeds
// cow/sheep, carrot breeds pig, seeds breed chicken (vanilla). Non-food for now
// (vanilla carrot edibility waits for a farming milestone, and keeping them
// non-food means RMB-feeding never collides with the M37 hold-to-eat gate).
extern ItemId Wheat;
extern ItemId Carrot;
extern ItemId Seeds;
// M39 spider drops (plain sprites). String is the primary drop (also a vanilla
// bow/lead ingredient); the spider eye is a player-kill secondary — non-food for
// now (vanilla spider eye poisons, which waits for a status-effect system, so it
// stays a sprite-only brewing ingredient like gunpowder).
extern ItemId String;
extern ItemId SpiderEye;
// RS1: redstone dust (sprite tile 124). Mined from redstone ore + dropped by
// breaking wire; placing it (RMB) lays the redstone wire block (placesBlock).
extern ItemId RedstoneDust;

// The three bow_pulling_0..2 draw-frame sprite tiles (atlas 109..111). The view
// model swaps the held bow's tile among these by draw charge (vanilla ItemBow
// model overrides). NOT registered as items — see Item.cpp.
inline constexpr uint16_t kBowPullingTiles[3] = {109, 110, 111};

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
