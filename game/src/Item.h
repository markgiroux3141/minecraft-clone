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

struct ItemDef {
    std::string name;
    uint16_t tile = 0; // atlas layer of the 2D sprite (UI icon + world drop)
    ToolClass tool = ToolClass::None;
    float efficiency = 1.0f; // dig-speed multiplier on matching blocks
    int maxDamage = 0;       // tool durability in uses; 0 = not damageable
    int maxStack = 64;       // tools stack to 1
    int tier = 0; // pickaxe harvest tier vs BlockDef::harvestLevel (M21):
                  // wood 0, stone 1, iron 2
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
