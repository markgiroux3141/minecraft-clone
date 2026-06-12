# Mod integration: BuildCraft + IndustrialCraft

Future goal: reimplement the gameplay systems of BuildCraft (automation: pipes,
engines, quarry) and IndustrialCraft 2 (electricity: EU network, machines,
nuclear power) natively in Voxcraft. This document is the entry point; the
per-mod breadcrumb guides are:

- [BUILDCRAFT_GUIDE.md](BUILDCRAFT_GUIDE.md) — where everything lives in the BC source
- [INDUSTRIALCRAFT_GUIDE.md](INDUSTRIALCRAFT_GUIDE.md) — where everything lives in the IC2 source

## Source locations

| Mod | Path | Version | Target MC |
|-----|------|---------|-----------|
| BuildCraft | `D:\Minecraft mods source code\Buildcraft` | 8.0.1-pre.2 (+ full 7.x-era code in `src_old_license/`) | 1.12.2 |
| IndustrialCraft | `D:\Minecraft mods source code\Industrialcraft` | "IC2 Refined" fork of classic IC2 (Tekkit Classic 3.1.2) | 1.2.5 |

Vanilla 1.12 reference source + assets: `D:\Minecraft source code` (see project
memory — personal use only).

## Completeness assessment (checked 2026-06-12)

### BuildCraft — complete

- ~1064 Java files of current (BC8) module code in `common/buildcraft/`, plus
  782 files of BC 7.x-era code in `src_old_license/buildcraft/` (which includes
  the **full robotics module with working robots** — BC8 only has zone-planner
  stubs).
- Full assets in `buildcraft_resources/assets/` — textures, block/item models,
  recipes (JSON), lang files, advancements.
- The three git submodules (`BuildCraftAPI/`, `BuildCraft-Localization/`,
  `BuildCraftGuide/`) were **empty when first checked**; they have since been
  cloned at their pinned commits (done 2026-06-12, needed network). The API
  (251 files, 27 packages) is the cleanest spec of every system — read it
  before the implementation code.

### IndustrialCraft — code complete, assets missing

- 283 Java files: full server-side gameplay logic for classic IC2 — EnergyNet,
  all generators, machines, reactor, cables, crops, tools, armor, terraformer,
  teleporter. The `ic2/api/` package (31 files) documents the contracts.
- **No assets at all**: no textures, sounds, or lang files anywhere in the repo,
  and `lib/Tekkit.jar` (a server jar) contains none either. Texture paths are
  referenced in code (e.g. `/ic2/sprites/block_cable.png`) but the PNGs don't
  exist here.
- **No client GUI rendering code**: container classes (slot layouts, server
  logic) are all present, but the `Gui*` screen classes they name are absent.
  GUI layouts can be reconstructed from the containers + IC2 wiki screenshots.
- This is a 1.2.5-era codebase: simpler than later IC2 (no E-net rework, EU is
  plain `int`), which is actually a good fit for reimplementation.

**Asset gap — RESOLVED (found locally 2026-06-12)**: the Tekkit Legends
modpack installed in the Technic launcher ships **IC2 Classic**, whose jar
contains the full classic asset set:

```
%APPDATA%\.technic\modpacks\tekkit-legends\mods\IC2.Classic.Version.1.1.3.5.jar
```

Its `assets/ic2/` tree (147 files) has been **extracted to
`D:\Minecraft mods source code\Industrialcraft\assets\ic2\`** (done
2026-06-12, spot-checked): 84 textures — including the *exact sprite sheets
the 1.2.5 source references by name* (`textures/sprites/block_cable.png`,
`block_generator.png`, `block_machine.png`, `block_machine2.png`,
`block_electric.png`, `item_0.png`, `crops_0.png`, ore singles) — plus 43
per-machine GUI backgrounds in `textures/guiSprites/` (GUIMacerator,
GUIGenerator, GUIElectricBlock, GUIMatter, …), 62 sounds in `sounds/`
(machine loops, reactor Geiger ticks), armor models, and `lang/en_US.lang`.
When the milestone lands, extend `scripts/import_mc_assets.py` to pull from
that folder (same personal-use-only, gitignored `assets/mc/` treatment as
vanilla assets).

Backup sources if more/different art is ever needed:
- **Tekkit Classic** via the already-installed Technic launcher — installs the
  original `industrialcraft2-1.97` *client* zip (the exact version our source
  fork matches).
- **CurseForge "Industrial Craft"** (official IC2 experimental for 1.12.2) —
  modern per-block texture layout, redrawn art for some machines.
- **CurseForge "IC2 Classic"** (Speiger's maintained port) — newer builds of
  the same mod found locally.

## Licensing

- BuildCraft: current code (`common/`, `BuildCraftAPI/`) is **MPL 2.0**
  (`LICENSE-NEW`); `src_old_license/` is MMPL 1.0.1 (`LICENSE`). Both permit
  studying/reuse with attribution; MPL 2.0 is file-level copyleft.
- IC2: **proprietary** — the original mod was never open-sourced; this fork is
  reconstructed source. Treat it the same as the vanilla source: a private
  reference for understanding exact mechanics, personal use only. Reimplement
  behavior, don't copy code verbatim into anything distributed.

## Era mismatch — decisions we own

The two sources are five Minecraft versions apart, so cross-mod conventions
must be decided by us, not copied:

- **Power units**: BC uses MJ (in BC8, fixed-point: 1 MJ = 1,000,000
  "microjoules" stored in a `long` — directly portable to `int64_t`). IC2 uses
  EU (plain `int`). They were never directly convertible; Tekkit-era community
  ratio was 1 MJ ≈ 2.5 EU. We can keep two deliberately-incompatible power
  systems (faithful, more interesting) or unify internally with display-only
  units. Decide at implementation time.
- **BC pipes vs IC2 cables** are unrelated systems and should stay separate.
- IC2 classic balance numbers (machine costs, storage sizes) are tuned for
  1.2.5 progression; BC8 numbers for 1.12 progression. Expect to retune.

## Mapping to Voxcraft architecture

Everything below is gameplay → lives in `game/src/`, renders only through
`vox::Renderer`, simulates in `OnTick` (20 TPS — same tick rate both mods
assume, so per-tick constants like EU/t and MJ/t transfer directly).

Prerequisites we don't have yet (as of M18) that both mods assume:

1. **Tile entities** — blocks with per-instance state + per-tick logic
   (inventories, energy buffers, progress bars). Needed by everything below.
2. **Block metadata/state** beyond block ID (orientation, machine variant).
3. **Crafting** — recipe registry; both mods bolt machine recipes onto it.
4. **GUI framework** — container/inventory screens (we have a hotbar; machines
   need openable inventories with slots and progress widgets).
5. **Item entities in world** already exist (M17/M18) — pipes need items
   rendered *inside* pipes, which is a separate lightweight path.

## Suggested implementation order

Each phase is independently shippable and testable:

1. **Foundations**: tile entity system, machine GUIs, crafting. (Vanilla-level
   work; reference furnace in the vanilla source.)
2. **IC2 power core**: cables + EnergyNet + generator + batbox + one consumer
   (electric furnace). Smallest closed loop that proves the energy graph.
   The 1.2.5 EnergyNet (`ic2/common/EnergyNet.java`) is a single readable file.
3. **IC2 machine set**: macerator/compressor/extractor/recycler + ore dusts —
   this is the ore-doubling progression loop players actually feel.
4. **BC transport**: item pipes (wood/stone/gold/iron/diamond) — flow model in
   `PipeFlowItems` + per-type behaviors. The most visually rewarding system.
5. **BC power + factory**: engines (MJ), quarry, pump, tank, refinery/distiller.
6. **Endgame layers** (pick by appetite): IC2 nuclear reactor, BC gates &
   logic, BC builders/blueprints, IC2 crops, BC robots (from `src_old_license`).

## Gotchas discovered during the survey

- BC8 is a **pre-release**; some subsystems (robotics, some silicon content)
  are stubs. For any system that feels incomplete in `common/`, check the same
  module in `src_old_license/buildcraft/` — the BC7 version is complete.
- IC2's `ic2/api/EnergyNet.java` is a reflection wrapper; the real logic is
  `ic2/common/EnergyNet.java` (note the `EnergyPath` inner class — that's the
  whole transmission algorithm).
- The IC2 fork's README documents deliberate behavior changes vs stock IC2
  (transformer 128× packet counts, geothermal buffering, luminator updates).
  When fidelity to original IC2 matters, check the README diff list first.
