# IndustrialCraft 2 source breadcrumbs

Source root: `D:\Minecraft mods source code\Industrialcraft` ("IC2 Refined" —
a server-side fork of classic IC2 for Tekkit Classic 3.1.2 / MC 1.2.5). All
paths below are relative to `src/main/java/`. See
[MOD_INTEGRATION.md](MOD_INTEGRATION.md) for the overall plan. This repo is
**code only** (no textures/sounds/lang, no GUI screen classes) — but the full
classic asset set was found locally; see [Assets](#assets) at the bottom.

## Orientation

| Package | What it is |
|---------|------------|
| `ic2/api/` | **Start here.** 31 files defining the contracts: energy net interfaces, `IElectricItem`, `IReactor`, `CropCard`, `IWrenchable`. |
| `ic2/common/` | All gameplay logic (227 files, flat package — everything is in here). |
| `ic2/platform/` | Client/server abstraction shims (mostly ignorable for porting). |
| `net/minecraft/server/mod_IC2.java` | Mod entry point: **every block/item registration, all ~300 crafting recipes, all balance config constants** in one giant file. The master index. |
| `ic2/bcIntegration22x/` | Tiny BuildCraft interop sample (geothermal accepting BC fuel). |

The fork's README lists its deliberate behavior changes vs stock IC2
(transformer 128× packet counts, geothermal 24k EU buffer, luminator update
rate). Check it when fidelity to original IC2 matters.

## EU energy network — the heart of the mod

| Thing | Path |
|-------|------|
| **The whole algorithm** | `ic2/common/EnergyNet.java` — one world-scoped graph; inner class `EnergyPath` holds precomputed source→sink paths with cumulative cable loss. Read this file end to end before designing ours. |
| Interfaces | `ic2/api/IEnergySource.java`, `IEnergySink.java`, `IEnergyConductor.java`, `IEnergyAcceptor.java`, `IEnergyEmitter.java`, `IEnergyTile.java` |

Core model (all EU values are plain `int`):

- Sources emit **packets** of EU (size = source's output, e.g. 32 EU for LV).
- The net pathfinds from source to all reachable sinks through conductors,
  caches paths, invalidates on block change.
- Each conductor adds loss (`getConductionLoss()`, EU per block traveled).
- **Tiers**: LV 32 / MV 128 / HV 512 / EV 2048 EU per packet. A sink or cable
  receiving a packet above its tier **burns up / explodes** — this is the
  signature IC2 failure mode and worth keeping.
- Uninsulated cables shock nearby entities (`entityLivingToShockEnergyMap`).

## Cables

`ic2/common/BlockCable.java` (13 variants by metadata) +
`TileEntityCable.java` (loss, insulation absorb/breakdown energy, CF-foam
covering). Detector/splitter cables: `TileEntityCableDetector.java`,
`TileEntityCableSplitter.java`.

Classic loss/capacity table is encoded in `getConductionLoss()` /
`getMaxCapacity()` switches: copper 0.2 EU/block (LV), gold 0.4 (MV), iron/HV
0.8, glass fibre 0.025 (HV, no shock), tin 0.025 (5 EU only).

## Generators

Base class with the fuel→storage→emit loop: `ic2/common/TileEntityBaseGenerator.java`.

| Generator | Tile | Output |
|-----------|------|--------|
| Generator (coal) | `ic2/common/TileEntityGenerator.java` | 10 EU/t |
| Geothermal (lava) | `ic2/common/TileEntityGeoGenerator.java` | 20 EU/t (fork: 24k EU buffer) |
| Solar | `ic2/common/TileEntitySolarGenerator.java` | 1 EU/t, sun/rain/biome checks |
| Wind | `ic2/common/TileEntityWindGenerator.java` | `windStrength × (height−64−obstruction) / 750` |
| Water mill | `ic2/common/TileEntityWaterGenerator.java` | passive from adjacent water / water-bucket fuel |
| **Nuclear reactor** | `ic2/common/TileEntityNuclearReactor.java` + `TileEntityReactorChamber.java`, API `ic2/api/IReactor.java` | 9×6 component grid, heat production/dissipation per uranium-cell pulse, hull heat → melt/explode. The famous reactor-planner math is all in the tile's tick. |

## Storage & transformers

| Block | Tile | Numbers |
|-------|------|---------|
| BatBox | `ic2/common/TileEntityElectricBatBox.java` | 40k EU, 32 EU/t, tier 1 |
| MFE | `ic2/common/TileEntityElectricMFE.java` | 600k EU, 128 EU/t, tier 2 |
| MFSU | `ic2/common/TileEntityElectricMFSU.java` | 10M EU, 512 EU/t, tier 3 |
| LV/MV/HV transformer | `ic2/common/TileEntityTransformer{LV,MV,HV}.java`, base `TileEntityTransformer.java` | step down by default; redstone = step up. Fork multiplies packet counts 128× (see README). |

All storage blocks share `TileEntityElectricBlock.java` (charge/discharge item
slots, emit when full, redstone modes).

## Machines

Tile hierarchy: `TileEntityBlock` → `TileEntityMachine` (redstone/active state)
→ `TileEntityElectricMachine` (EU buffer + progress loop). Recipe lists are
static fields on each tile, filled from `mod_IC2.java` /
`ic2/api/Ic2Recipes.java`.

| Machine | Tile | Role |
|---------|------|------|
| Macerator | `ic2/common/TileEntityMacerator.java` | ore → 2 dust (**the** progression hook) |
| Electric furnace | `ic2/common/TileEntityElecFurnace.java` | faster smelting |
| Compressor | `ic2/common/TileEntityCompressor.java` | plates, carbon, diamonds |
| Extractor | `ic2/common/TileEntityExtractor.java` | rubber ×3 from resin |
| Recycler | `ic2/common/TileEntityRecycler.java` | anything → 12.5% scrap |
| Induction furnace | `ic2/common/TileEntityInduction.java` | dual-slot, heat ramp-up |
| Mass fabricator | `ic2/common/TileEntityMatter.java` | 1M EU → UU-matter, scrap amplifier |
| Miner | `ic2/common/TileEntityMiner.java` | auto-mining with mining pipe, drill + OD/OV scanner |
| Pump | `ic2/common/TileEntityPump.java` | fluid → cells, pairs with miner |
| Canner | `ic2/common/TileEntityCanner.java` | food cans, fuel cans |
| Terraformer | `ic2/common/TileEntityTerra.java` + `ItemTFBP*.java` | biome/terrain rewriting blueprints |
| Teleporter | `ic2/common/TileEntityTeleporter.java` + `net/minecraft/server/ic2_ServerTeleportHelper.java` | EU cost scales with distance & inventory weight |
| Tesla coil, magnetizer, luminator, cropmatron | `TileEntityTesla.java`, `TileEntityMagnetizer.java`, `TileEntityLuminator.java`, `TileEntityCropmatron.java` | |

GUI note: each machine's `Container*.java` (e.g. `ContainerNuclearReactor.java`)
encodes the slot layout — that plus the IC2 wiki is enough to rebuild screens;
the original `Gui*` classes are not in this repo.

## Items, tools, armor

| Thing | Path |
|-------|------|
| Master item/block registry (IDs, every ItemStack) | `ic2/common/Ic2Items.java` |
| Electric item charge/discharge logic (NBT charge, tiers, transfer limits) | `ic2/common/ElectricItem.java`, API `ic2/api/IElectricItem.java` |
| Drill / diamond drill / chainsaw / electric hoe | `ic2/common/ItemElectricTool*.java` |
| Mining laser (fires `EntityMiningLaser`) | `ic2/common/ItemToolMiningLaser.java` |
| Wrench (machine rotation/pickup, electric variant) | `ic2/common/ItemToolWrench*.java`, API `ic2/api/IWrenchable.java` |
| Batteries / energy crystal / lapotron | `ic2/common/ItemBattery*.java` |
| Nano suit, quantum suit (powered armor with abilities) | `ic2/common/ItemArmorNanoSuit.java`, `ItemArmorQuantumSuit.java` |
| Jetpacks, lappack/batpack, solar helmet | `ic2/common/ItemArmorJetpack*.java`, `ItemArmorLappack.java`, `ItemArmorBatpack.java`, `ItemArmorSolarHelmet.java` |
| Treetap + sticky resin (rubber economy) | `ic2/common/ItemTreetap.java`, `ItemResin.java`, `BlockRubWood.java` |

## World content

- **Ores**: copper, tin, uranium — registered/configured in `mod_IC2.java`
  (`generateOre*` flags and vein parameters).
- **Rubber trees**: `ic2/common/WorldGenRubTree.java`, `BlockRubWood.java`
  (resin spots), `BlockRubSapling.java`, `BlockRubLeaves.java`.
- **Explosives**: ITNT, nuke, dynamite — `Block*` + `Entity*` pairs,
  `ExplosionIC2.java` (custom explosion honoring `ExplosionWhitelist`).
- **Crops**: full breeding/genetics system — `TileEntityCrop.java`,
  `IC2Crops.java` registry, 16 `Crop*.java` cards, API `CropCard.java` /
  `BaseSeed.java`. Self-contained; can be ported late or skipped.

## Recipes & balance constants

- All shaped/shapeless crafting recipes: `registerCraftingRecipes()` in
  `net/minecraft/server/mod_IC2.java` (~300 recipes, hardcoded).
- Machine recipes (macerator/compressor/extractor lists): `init()` methods on
  each tile + `ic2/api/Ic2Recipes.java` entry points.
- Balance knobs (EU values, ore gen, explosion power, …): the big static-field
  block at the top of `mod_IC2.java`.

## Assets

Extracted (2026-06-12) to `D:\Minecraft mods source code\Industrialcraft\assets\ic2\`
— 147 files sourced from the IC2 Classic jar shipped with Tekkit Legends
(`%APPDATA%\.technic\modpacks\tekkit-legends\mods\IC2.Classic.Version.1.1.3.5.jar`).

Layout under `assets/ic2/`:

| Path | Contents |
|------|----------|
| `textures/sprites/` | The 16×16-grid sprite sheets the source code indexes into, with **matching filenames** (`block_cable.png`, `block_generator.png`, `block_machine.png`, `block_machine2.png`, `block_electric.png`, `item_0.png`, `crops_0.png`, …) plus storage-block sheets (Batbox/MFE/MFSU) |
| `textures/sprites/single/` | Per-block ore textures (`oreCopper/oreTin/oreUranium.png`) |
| `textures/guiSprites/` | 43 per-machine GUI backgrounds (`GUIMacerator.png`, `GUIGenerator.png`, `GUIElectricBlock.png`, `GUIMatter.png`, `GUIMiner.png`, …) — pairs with each `Container*.java` slot layout |
| `textures/models/armor/` | Nano/quantum/jetpack worn-armor textures |
| `sounds/` | 62 OGGs: machine op/loop sounds, generator loops, reactor loop + Geiger EU ticks |
| `lang/en_US.lang` | Display names for every block/item |

Texture-index mapping: each block class's `getTextureFile()` names the sheet
and its `getBlockTextureFromSideAndMetadata()` returns an index into the
16-column grid (index = row*16+col, 16 px cells).
