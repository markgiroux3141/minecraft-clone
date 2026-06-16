# Spec: native BuildCraft + IndustrialCraft integration

**Status:** design spec, not scheduled. Pick up when the survival arc (M30–M33)
is done and the team wants the automation/tech arc.

This is the *design* document — the architecture, the shared/separate decisions,
and the order of work. It assumes the source surveys in
[MOD_INTEGRATION.md](MOD_INTEGRATION.md), [BUILDCRAFT_GUIDE.md](BUILDCRAFT_GUIDE.md),
and [INDUSTRIALCRAFT_GUIDE.md](INDUSTRIALCRAFT_GUIDE.md) (where each mechanic lives
in the source, source locations, licensing, asset locations). Read those for
"where is X in the source"; read this for "how do we build it."

Goal: reimplement the *gameplay* of BuildCraft (automation — pipes, engines,
quarry) and IndustrialCraft 2 (electricity — EU network, machines, nuclear) as
native Voxcraft systems. These were mods; we are not bound to treat them as
isolated add-ons. Where it is more elegant, they share infrastructure.

---

## Headline: most of the prerequisites already exist

The breadcrumb docs were written at M18 and list five "prerequisites we don't
have yet." As of M30, four of the five exist — built incidentally by the
survival milestones. The furnace (M21) is, in all but name, the first tile
entity *and* the first machine-with-GUI.

| Prerequisite (per MOD_INTEGRATION.md) | Status at M30 | Where it lives |
|---|---|---|
| Tile entities (per-block state + per-tick logic + persistence) | Exists, **hardcoded for one block** | `FurnaceState` in [Furnace.h](../../game/src/world/Furnace.h); `m_furnaces` map + `TickFurnaces()` + `SaveFurnaces()` (furnaces.dat sidecar) in [World.h](../../game/src/world/World.h) |
| Block metadata beyond ID | Exists | M24 `Chunk::m_meta` COW layer; `SetBlock(pos,id,meta)` / `GetMeta(wx,wy,wz)` |
| Crafting / recipe registry | Exists | [Crafting.h](../../game/src/Crafting.h) — shaped/shapeless, ingredient alternatives, mirror |
| GUI container framework (slots + progress widgets) | Exists | [InventoryScreen.h](../../game/src/ui/InventoryScreen.h) — `DrawFurnace` already does input/fuel/output slots + flame/arrow progress |
| Item entities in world | Exists | M18 `World::ItemEntity` |

The remaining gap is **fluids inside tanks/pipes/cells** (distinct from the
block-based water/lava flow we have) — see Prerequisites below.

**Implication for scale:** the hard part is not inventing infrastructure. It is
(1) generalizing the furnace into a reusable framework, then (2) pouring content
(machines, recipes, balance numbers, textures) into it. The source is large
(~1064 BC8 + 782 BC7 files, 283 IC2 files), but most of that is rendering, Forge
plumbing, and netcode we don't port. The actual mechanics are concentrated in a
handful of files called out in the breadcrumb guides.

---

## Prerequisites — complete these FIRST

Do not start machine content until these land. They are the spine everything
else hangs from; building a machine before them means rework.

### P0. Generalize the furnace into a `BlockEntity` framework  — REQUIRED, the keystone

Lift the existing furnace code up one level of abstraction. A registry of
block-entity types, each providing:

- per-instance state (the furnace's `FurnaceState` becomes one implementation),
- `OnTick(World&, pos)` at 20 TPS,
- serialize / deserialize (generalize the `furnaces.dat` sidecar to a typed
  block-entity store),
- a "drop contents on break" hook (furnace spill already does this),
- an "open GUI" hook (furnace screen already does this).

**Acceptance:** re-express the furnace through the framework with *zero visible
behaviour change*. That is the proof the abstraction is right. Risk is low — the
patterns are all proven by the existing furnace; this is mostly a refactor.

This is the only true blocker. P1–P3 below can land in the same arc or be
folded into the first machine slice, but P0 must come first.

### P1. Capability model (per-face item / fluid / energy)  — REQUIRED for "deep integration"

The genuinely elegant cross-mod glue, and the thing that makes this "deeply
integrated" rather than "two mods bolted on." In 1.12 Forge both mods already
speak this language: a block exposes *capabilities* per face. Give a Voxcraft
`BlockEntity` the ability to answer, per face:

- can I accept / provide **items**? (generalizes the furnace's input/output/fuel slots)
- can I accept / provide **fluid**?
- can I accept / provide **energy**?

With this, BC pipes feed IC2 machines, IC2 cables power anything, and the
furnace + a quarry + a macerator interoperate without knowing about each other.
Without it, every pair of systems needs bespoke glue. Build it with P0.

### P2. Container / slot generalization  — REQUIRED before the 2nd machine

`InventoryScreen::DrawFurnace` is already an N-slot container with progress
overlays. Generalize to: "N typed slots (input / output / fuel / charge) + M
progress widgets + a GUI background image," so each new machine screen is data,
not new C++. IC2 ships per-machine GUI backgrounds (`textures/guiSprites/`,
e.g. `GUIMacerator.png`) that pair 1:1 with each `Container*.java` slot layout.

### P3. `FluidStack` primitive  — REQUIRED before fluids-in-pipes / tanks / cells

A `{fluid id, millibuckets}` value type mirroring `ItemStack`. Distinct from our
block-based liquid flow (water/lava as blocks). Needed by BC tanks/pump/refinery,
fluid pipes, and IC2 cells/cans. M26's buckets already gesture at this.

**Decision to make:** include fluids in the first machine slice, or defer?
**Recommendation: defer.** Start with items + EU only (Phase 1–2 below need no
fluids), and add P3 when Phase 4 (BC engines/factory) pulls it in.

### Codebase-readiness gate (not a build task)

- The survival arc (M30 health, M31 entity renderer, M32 mobs, M33 armor) should
  be landed and verified first — not because automation depends on it, but so the
  tile-entity refactor (P0) happens against a settled `World` rather than racing
  other structural churn. M31's entity renderer also overlaps nothing here, so
  there is no contention once it ships.

---

## Architecture: one shared spine, two separate "physics"

Share the **plumbing**. Keep the **physics** separate. Over-unifying the physics
is the main trap.

### Share (build once, in `game/src/`)

1. **`BlockEntity` framework** (P0) — every machine, pipe, engine, tank, and the
   furnace are instances.
2. **Capability model** (P1) — the cross-system interop layer.
3. **`FluidStack`** (P3) — shared fluid primitive.
4. **Container/slot framework** (P2) — shared machine GUIs.
5. **Connectivity / graph scaffold (optional, shared carefully).** Both the IC2
   EU EnergyNet and BC pipe networks are graphs over adjacent blocks, rebuilt
   incrementally on block edits. Share *only* the connectivity tracking +
   dirty/rebuild-on-edit + BFS traversal. **Do NOT** share the transport
   semantics on top (see below).

### Keep separate

- **EU (IC2) vs MJ (BC) power.** Never convertible historically, and they fail
  differently: EU *over-tier shocks/explodes* (signature IC2 feel); MJ is
  continuous fixed-point with engine heat. Unifying into one number discards both
  identities. **Decision — recommended: keep two real power systems.** Store each
  as `int64_t` micro-units. If they ever need to interact, build one explicit
  *converter/transformer block*, not a universal currency.
- **Pipes (BC) vs cables (IC2).** Unrelated systems. They share only the
  connectivity scaffold above, never the flow logic: items-in-pipes are *moving
  objects* with position/speed; EU is *discrete packets*. Different code paths.

---

## Design decisions the owner must settle (before build)

1. **Power units:** dual (recommended) vs unified internal currency. → dual.
2. **Fluids:** first slice vs deferred. → deferred (add at Phase 4).
3. **Fidelity vs retune:** IC2-classic numbers are 1.2.5-tuned, BC8 numbers are
   1.12-tuned, our world is its own thing. Treat all source balance numbers as
   *starting points*; expect to retune EU/t, MJ/t, storage sizes, ore veins.

---

## Phasing

Each phase is an independently shippable vertical slice, matching the
milestone-by-milestone workflow. The order refines MOD_INTEGRATION.md's
"suggested implementation order" given the current (M30) state.

| Phase | Slice | Size | Notes |
|---|---|---|---|
| **0** | Prerequisites P0–P2 (framework, capabilities, containers) | Medium, low-risk refactor | Ships by re-expressing the furnace through it — zero visible change. The only hard blocker. |
| **1** | IC2 power core: copper cable + EnergyNet + coal generator + BatBox + electric furnace | Medium | Smallest closed loop that proves the energy graph end-to-end. Spec = IC2's single `EnergyNet.java`. |
| **2** | IC2 machine set: macerator / compressor / extractor / recycler + ore dusts | Medium (mostly content) | The ore-doubling loop players actually feel. High payoff on the Phase-1 spine. |
| **3** | BC item pipes: wood/stone/gold/iron/diamond behaviours | Medium-large | The travelling-item flow + junction routing is the most new-code-per-feature. Most visually rewarding. |
| **4** | BC power + factory: engines (MJ + heat) + quarry + pump + tank | Large | Pulls in P3 (fluids). Quarry is a multiblock + area-loading concern. |
| **5+** | Endgame, pick by appetite: IC2 reactor, BC gates/logic, blueprints, IC2 crops, BC7 robots | Very large, fully optional | Reactor math, gate expression language, blueprint snapshots, robot AI — each a milestone or more. |

**Reachable first target:** Phases 0 → 1 → 2 (a macerator-and-cables vertical
slice, ~3 milestones) proves out the entire shared architecture. Everything past
that is content scaling on a spine you already trust.

---

## Effort sizing (rough, relative)

- **Phase 0:** medium but mostly refactor, low risk — patterns proven by the furnace.
- **Phases 1–2:** medium each; EnergyNet is contained, machines are repetitive content.
- **Phase 3:** medium-large — moving-item flow + junction routing is the most novel code.
- **Phase 4:** large — quarry multiblock + fluids subsystem.
- **Phase 5+:** very large, optional.

---

## Asset pipeline notes (already half-solved)

- BC uses vanilla-1.12 JSON models/blockstates — `scripts/import_mc_assets.py`
  already understands that format. BC textures/models/recipes live under
  `buildcraft_resources/assets/buildcraft<module>/`.
- IC2 uses older sprite-sheet indexing (sheet + `row*16+col` index). The classic
  asset set was located and extracted (see INDUSTRIALCRAFT_GUIDE.md §Assets):
  sprite sheets with matching filenames, 43 GUI backgrounds, 62 sounds, lang.
  When this lands, extend the import script to pull from
  `D:\Minecraft mods source code\Industrialcraft\assets\ic2\` with the same
  gitignored, personal-use-only treatment as vanilla assets.

## Licensing reminder

BC current code is MPL 2.0 (file-level copyleft, attribution); BC7
`src_old_license/` is MMPL. IC2 is proprietary reconstructed source. For all
three: **reimplement behaviour, do not copy code verbatim** into the distributed
repo. The sources are a private reference for exact mechanics. See
MOD_INTEGRATION.md §Licensing.
