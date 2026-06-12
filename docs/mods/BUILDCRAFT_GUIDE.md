# BuildCraft source breadcrumbs

Source root: `D:\Minecraft mods source code\Buildcraft` (BC 8.0.1-pre.2, MC
1.12.2, Forge). All paths below are relative to that root. See
[MOD_INTEGRATION.md](MOD_INTEGRATION.md) for the overall plan.

## Orientation

| Directory | What it is |
|-----------|------------|
| `BuildCraftAPI/api/buildcraft/api/` | **Start here.** Clean interfaces for every system (27 packages: `mj`, `transport`, `statements`, `schematics`, `robots`, `fuels`, `recipes`, …). Restored git submodule, pinned commit. |
| `common/buildcraft/` | Current BC8 implementation, split by module (below). |
| `src_old_license/buildcraft/` | Complete BC 7.x code (MMPL license). Use when BC8 is stubby — especially **robotics**. |
| `buildcraft_resources/assets/buildcraft<module>/` | Textures, JSON models/blockstates, JSON recipes, `lang/en_US.lang`, advancements. |
| `BuildCraft-Localization/` | Translations (restored submodule). |
| `sub_projects/expression/` | Standalone expression-language library used by gates. |

Modules under `common/buildcraft/`: `core`, `lib` (shared utils — biggest),
`energy`, `transport`, `factory`, `builders`, `silicon`, `robotics`. Each has
`BC<Module>Blocks/Items/Recipes/Statements/...java` registration files at its
root — these are the **index of what exists** in a module; read them first.

## Energy (MJ)

The unit: 1 MJ = 1,000,000 microjoules, stored as `long` (`MjAPI.MJ`). Port as
`int64_t` micro-MJ — no floats in power math.

| Thing | Path |
|-------|------|
| API: connector/receiver/provider interfaces, `MjBattery` | `BuildCraftAPI/api/buildcraft/api/mj/` |
| Engine base (heat, power stages, pumping animation) | `common/buildcraft/lib/engine/TileEngineBase_BC8.java` |
| Redstone engine (free, weak) | `common/buildcraft/core/tile/TileEngineRedstone_BC8.java` |
| Stirling/stone engine (solid fuel) | `common/buildcraft/energy/tile/TileEngineStone_BC8.java` |
| Combustion/iron engine (liquid fuel, coolant, explodes) | `common/buildcraft/energy/tile/TileEngineIron_BC8.java` |
| Fuel registry (which fluids burn, MJ/tick, total MJ) | `BuildCraftAPI/api/buildcraft/api/fuels/` + `common/buildcraft/energy/BCEnergyRecipes.java` |

Engine heat model: heat 20→250, ideal 100; engines change color (blue→red) and
explode past max. Constants live in `TileEngineBase_BC8`.

## Pipes (transport module)

Architecture = three orthogonal pieces, combined in `PipeDefinition`:
**behaviour** (per-material logic), **flow** (what travels: items/fluids/power),
**pluggables** (things attached to pipe sides: gates, facades, lenses).

| Thing | Path |
|-------|------|
| API (read first): `IPipe`, `PipeDefinition`, `PipeFlow`, `PipeBehaviour` | `BuildCraftAPI/api/buildcraft/api/transport/pipe/` |
| Pipe core class | `common/buildcraft/transport/pipe/Pipe.java` |
| Item flow — items move as `TravellingItem` with position/speed | `common/buildcraft/transport/pipe/flow/PipeFlowItems.java`, `TravellingItem.java` |
| Fluid flow | `common/buildcraft/transport/pipe/flow/PipeFlowFluids.java` |
| Power (kinesis) flow | `common/buildcraft/transport/pipe/flow/PipeFlowPower.java` |
| Per-material behaviors (wood=extract, iron=one-way, gold=speed, diamond=sort, obsidian=vacuum, emzuli/daizuli, void, …) | `common/buildcraft/transport/pipe/behaviour/PipeBehaviour*.java` (23 classes) |
| Pipe block/tile (one block hosts any pipe) | `common/buildcraft/transport/block/BlockPipeHolder.java`, `common/buildcraft/transport/tile/TilePipeHolder.java` |
| Registration of all pipe types | `common/buildcraft/transport/BCTransportPipes.java` |
| Pipe item-in-pipe rendering | `common/buildcraft/transport/client/render/PipeFlowRendererItems.java` |

Key mechanic to preserve: items in pipes are *not* inventory slots — they're
moving objects with insertion time, speed (gold accelerates, sandstone/cobble
friction), and direction resolution at junctions (wood pipes extract only with
engine power; round-robin/weighted choice at splits).

## Gates & logic (silicon module)

| Thing | Path |
|-------|------|
| Statement API (triggers/actions/parameters) | `BuildCraftAPI/api/buildcraft/api/statements/` |
| Gate evaluation engine | `common/buildcraft/silicon/gate/GateLogic.java` |
| Gate tiers/variants (material × logic AND/OR × slot count) | `common/buildcraft/silicon/gate/EnumGateMaterial.java`, `GateVariant.java` |
| Trigger/action implementations | `common/buildcraft/core/statements/`, `common/buildcraft/transport/statements/` |
| Pluggables: gate, facade, lens, timer, pulsar, light sensor | `common/buildcraft/silicon/plug/Pluggable*.java` |
| Assembly table + laser (crafts chipsets/gates with MJ lasers) | `common/buildcraft/silicon/tile/TileAssemblyTable.java`, `TileLaser.java` |

## Factory machines

| Machine | Tile | Notes |
|---------|------|-------|
| Pump | `common/buildcraft/factory/tile/TilePump.java` | Tube descends, drains fluid bodies breadth-first |
| Mining well | `common/buildcraft/factory/tile/TileMiningWell.java` | Straight-down digger |
| Tank | `common/buildcraft/factory/tile/TileTank.java` | Vertical stacking merges storage |
| Auto workbench | `common/buildcraft/factory/tile/TileAutoWorkbenchBase.java` | Powerless slow auto-crafting |
| Distiller | `common/buildcraft/factory/tile/TileDistiller_BC8.java` | Oil → gaseous/liquid fractions (BC8's refinery) |
| Heat exchanger | `common/buildcraft/factory/tile/TileHeatExchange.java` | Multiblock, heats/cools fluids |
| Flood gate, chute | `common/buildcraft/factory/tile/TileFloodGate.java`, `TileChute.java` | |

BC7's classic single-block **refinery** (oil→fuel) is in
`src_old_license/buildcraft/factory/TileRefinery.java` if the BC8
distiller/heat-exchanger chain is more than we want.

## Quarry & builders

| Thing | Path |
|-------|------|
| Quarry (the headline machine) | `common/buildcraft/builders/tile/TileQuarry.java` — frame building, drill movement, 24k MJ battery, chunkloading |
| Volume markers (area selection) | `common/buildcraft/core/tile/TileMarkerVolume.java`, `common/buildcraft/core/marker/` |
| Filler + geometric patterns (box, pyramid, sphere, stairs, 2D shapes) | `common/buildcraft/builders/tile/TileFiller.java`, `common/buildcraft/builders/snapshot/pattern/Pattern*.java` |
| Architect table → snapshot → builder pipeline | `common/buildcraft/builders/tile/TileArchitectTable.java`, `TileBuilder.java` |
| Snapshot/blueprint/template data model | `common/buildcraft/builders/snapshot/Snapshot.java`, `Blueprint.java`, `Template.java` |
| Schematic per-block placement rules | `common/buildcraft/builders/snapshot/SchematicBlock*.java` + JSON rules in `buildcraft_resources/assets/buildcraftbuilders/compat/` |

## Oil & fluids

| Thing | Path |
|-------|------|
| Oil/fuel/residue fluid definitions (density, viscosity, colors) | `common/buildcraft/energy/BCEnergyFluids.java` |
| Oil worldgen (wells, geysers, oil biomes) | `common/buildcraft/energy/generation/` |
| Fluid framework | `common/buildcraft/lib/fluid/` |

## Robots (BC7 only — `src_old_license`)

BC8 robotics is a stub (zone planner only). The complete system — robot
entities, docking stations on pipes, AI task boards, charging — is
`src_old_license/buildcraft/robotics/` (123 files) with its API contracts in
`BuildCraftAPI/api/buildcraft/api/robots/` and `boards/`.

## Assets cheat sheet

Per-module under `buildcraft_resources/assets/`:
`textures/blocks|items|gui/` (PNGs), `models/` + `blockstates/` (JSON, vanilla
1.12 format — same format our `import_mc_assets.py` pipeline already
understands), `recipes/` (JSON), `lang/en_US.lang`. Pipe textures:
`assets/buildcrafttransport/textures/pipes/`. GUI backgrounds:
`assets/buildcraft<module>/textures/gui/`.
