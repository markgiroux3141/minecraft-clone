# Voxcraft Architecture

A Minecraft-style voxel game built on a custom C++ engine. Singleplayer only by
design, but simulation and rendering stay strictly separated so systems remain
testable and replaceable.

## Layering

```
┌─────────────────────────────────────────┐
│ game/      Voxcraft — blocks, world     │
│            gameplay, UI                 │
├─────────────────────────────────────────┤
│ engine/    vox:: — reusable, knows      │
│            nothing about the game       │
│  ├─ core/      app loop, log, assert    │
│  ├─ platform/  window, input (GLFW)     │
│  └─ renderer/  GL backend facade        │
├─────────────────────────────────────────┤
│ third_party/  glad (vendored)           │
│ FetchContent  glfw, glm, spdlog         │
└─────────────────────────────────────────┘
```

Rules:
- Dependencies point downward only. `engine/` never includes from `game/`.
- Game code never calls OpenGL directly — everything goes through
  `vox::Renderer` (this is the seam for a future Vulkan backend).
- Simulation runs at a fixed 20 TPS (`OnTick`); rendering is uncapped
  (`OnRender`) and receives an interpolation alpha. Anything that moves should
  store previous+current state and interpolate at render time.

## Main loop

`vox::Application::Run()` implements the classic fixed-timestep accumulator:
poll events → run as many 50 ms ticks as the accumulator allows (capped to
avoid spiral-of-death after stalls) → render once with `alpha = leftover/tickDt`
→ swap.

## Roadmap (milestones)

- **M0 — Platform skeleton** ✅: window, GL 4.6 debug context, fixed-timestep
  loop, input, logging, clear color.
- **M1 — Render core** ✅: Shader, Buffer, VertexArray, Texture2D abstractions
  (GL 4.5 direct state access); fly camera; textured debug cube with
  tick-interpolated rotation.
- **M2 — World core** ✅: block registry (data-driven), 16³ chunk storage
  (palette compression later), culled per-face meshing, block textures in a
  GL texture array (not an atlas — no mip bleeding, UVs can tile for greedy
  meshing). World code lives in `game/src/world/`, namespace `vc`.
- **M3 — Chunk streaming** ✅: `vc::World` streams chunks around the camera
  (nearest-first, per-frame budgets, unload at radius+1), FastNoiseLite FBm
  heightmap terrain, seam-aware meshing (chunks mesh only once all neighbors
  have data), frustum culling via `vox::Frustum`.
- **M4 — Threaded pipeline** ✅: `vox::ThreadPool` worker pool runs
  generation + meshing jobs against immutable chunk snapshots; the main
  thread drains completion queues and only uploads buffers. View radius
  raised to 12.
- **M5 — Meshing quality** ✅: greedy meshing (faces merge on texture layer +
  AO key, UVs tile via the texture array), classic corner AO baked per
  vertex with AO-aware quad diagonals. ~10x triangle reduction.
- **M6 — Player** ✅: tick-simulated AABB walk physics (render-interpolated)
  + noclip fly mode (F toggles), DDA raycast break/place with versioned
  copy-on-write chunk edits and targeted-block outline.
- **M7 — Lighting** ✅: BFS flood-fill sky + block light computed per column
  on workers (3x3 column input), smooth per-vertex light baked next to AO,
  emissive blocks (glowstone), light-aware streaming radii.
- **M8 — Persistence** ✅: region-file saves of edited chunks (RLE blobs,
  atomic rewrites, seed manifest), loaded on stream-in before falling back
  to the generator; save on unload + 30 s autosave + quit flush.
- **M9 — Scale** ✅: one multi-draw-indirect call for all chunk meshes
  (`vox::MeshPool` + gl_DrawID SSBO), Minecraft-style cave-culling
  occlusion (face-connectivity BFS from the eye), half-res LOD shell out
  to ~512 blocks (downsampled real terrain, same mesher/pool, 2x scale).
- **M10 — UI/menus** ✅: batched 2D overlay renderer (`vox::UiRenderer`:
  rects, bitmap-font text, texture-array tiles in one draw), crosshair +
  hotbar HUD, pause menu with a Title/Playing/Paused state machine, title
  screen with world select / new-world creation (random seed), player
  position/look/mode persisted in the save manifest.
- **M11 — Gameplay depth** ✅: sand/log/leaves/water blocks, beach band,
  deterministic cell-hashed trees (seam-safe, gentest-guarded), water to
  sea level with a second blended mesh stream (chunk + LOD), look-
  direction swimming, day/night cycle (persisted world time, sky-dome
  gradient + sun disc, time-driven sun light, distance + underwater fog).
- **M12 — Perf polish** ✅: ChunkVertex packed from 48 B of floats into
  two uint32s (8 B, ~6x smaller pool + uploads; bit-decoded in
  chunk.vert), GPU mesh uploads budgeted at 2 MB/frame with versioned
  carry-over.
- **M13 — Block updates** ✅: scheduled block ticks (every SetBlock wakes
  its neighborhood), falling sand as render-interpolated entities with
  versioned mesh handover, and Minecraft-1.12-parity water flow (7 flow
  levels as appended BlockIds, slope-seeking spread, infinite sources,
  source-vs-flow sheeting rules) with corner-sampled partial-height
  rendering packed into spare vertex bits.
- **M14 — Real Minecraft assets** ✅: import pipeline
  (`scripts/import_mc_assets.py`) from the user's local 1.12 source into
  gitignored `assets/mc/` (zero distribution; committed placeholders
  remain the fallback): block atlas with baked biome tints, the real
  proportional 8px font (UiRenderer scans per-glyph advances), GUI
  crosshair/hotbar/buttons via a new DrawImage sprite-sheet mode,
  textured sun + phased moon on additive sky billboards, and a night
  lighting rework (moonlight floor, moon-directional diffuse, blue cast).
- **M15 — Caves + biomes** ✅: vanilla-1.12 worm caves (MapGenCaves port
  with an exact java.util.Random clone, analytic ocean-breach test,
  cave-aware tree gating via a carve mask), temperature/moisture climate
  biomes (desert/plains/forest/snowy + new snowy-grass block), and
  biome-driven topology (vanilla per-biome base/variation params blended
  with the parabolic 5x5 kernel, ruggedness-ramped hills variants,
  alpine snow line).
- **M16 — Flora & decoration** ✅: alpha-tested cutout pass inside the
  existing opaque stream (transparent "fancy" leaves; per-block light
  opacity adds canopy shade and depth-faded underwater skylight),
  cross-meshed plants (tall grass, dandelion, poppy, dead bush —
  biome-scattered, raycast-targetable, crushed by water/sand, popped by
  support ticks), and tree species (birch in forests, conical spruce in
  snow and on alpine caps, 1-3-tall cactus columns on desert sand).
- **M17 — Survival I: items & inventory** ✅: ItemStack/36-slot
  Inventory model behind the hotbar (counts on the HUD, placing
  consumes, breaking instant-picks-up as the M18 stopgap), inventory
  persistence as a level.dat manifest line, and the E-key inventory
  screen — vanilla's 176x166 container panel (slot grid from
  ContainerPlayer) plus a creative-style block palette, with vanilla
  PICKUP mouse rules (left whole-stack/swap/merge, right half /
  place-one).
- **M18 — Survival II: mining feel** ✅: vanilla per-block hardness +
  hold-to-break (digSpeed/hardness/30 per tick, water/airborne x1/5
  penalties, 5-tick hit delay) with destroy_stage_0..9 crack overlays
  (alpha-tested entity cube), block drops as item entities (vanilla
  EntityItem physics: scatter spawn, drag/friction, 0.5-block merge,
  5-min despawn, grown-AABB pickup into the inventory), a vanilla-ish
  drop table (stone -> new cobblestone block, grass -> dirt,
  leaves/tall grass -> nothing), Q-throw and inventory-screen throws.
  Fly mode breaks instantly and drops nothing (creative-style).
- **M19 — Survival III: crafting** ✅: unified item-id space (blocks
  < 1024, ItemRegistry above — sticks + wood/stone pickaxe/axe/shovel),
  shaped/shapeless recipe registry (translation + mirror, ingredient
  alternatives), the 2x2 grid + result in the inventory screen and a
  placeable crafting-table block whose RMB opens the 3x3
  (gui/container/crafting_table.png), new planks block, tools hooking
  M18's dig path (efficiency 2x/4x on matching blocks, vanilla pickaxe
  gating: stone family digs /100 and drops nothing bare-handed) with
  durability (59/131 uses, vanilla bar, persisted per stack), and flat
  sprite rendering for non-block item drops.
- **M20 — Game feel** ✅: block-break particles (ParticleDigging port:
  hit chips at the dug face each tick + the 4x4x4 destroy burst,
  quarter-tile jittered UVs, vanilla physics, one streamed billboard
  batch) and the first-person view model (held block as a mini cube /
  item as a flat sprite / bare Steve arm when empty-handed, vanilla
  ItemRenderer transform chains: 6-tick swing looped while digging,
  +-0.4-per-tick equip dip on slot change, drawn over a cleared depth
  buffer).
- **M21 — Ores + furnace/smelting** ✅: coal/iron ore worldgen
  (WorldGenMinable port, seam-deterministic 3x3 origin replay), pickaxe
  harvest tiers (wood 0 / stone 1 / iron 2; iron ore needs stone+), the
  furnace as the first block entity (World-owned position-keyed state,
  ticked at 20 TPS, lit/unlit block-id swap, furnaces.dat sidecar
  persistence, spill-on-break, vanilla GUI with flame/arrow progress),
  smelting (iron ore -> ingot, sand -> new glass block, cobble ->
  stone), coal/wood/stick fuels, and iron tools (efficiency 6,
  250 uses). Follow-up: floor-standing torches (light 14, vanilla
  4-inset-plane mesh riding spare packed-vertex bits, coal + stick ->
  4); wall mounting waits for block orientation data.
- **Backlog**: deeper world (kWorldHeightChunks 4 → 8), audio engine,
  tall-grass wheat seeds (1/8 chance, BlockTallGrass — pairs
  with farming), animated water, lava, stars, world-list scrolling,
  settings screen, vanilla's 14/16 cactus inset, 3D-extruded item
  sprites in hand (flat quad for now), view bobbing, block orientation
  data (table/furnace fronts, wall torches).
- **Long-term**: native BuildCraft + IndustrialCraft systems (pipes, EU
  power network, machines) — survey, source breadcrumbs, and phased plan
  in `docs/mods/MOD_INTEGRATION.md`.

Each milestone lands as a vertical slice that runs; no big-bang integration.

## Conventions

- C++23, MSVC `/W4 /permissive-`. Format with the repo `.clang-format`.
- Engine code lives in `namespace vox`, files under `engine/src/vox/<module>/`.
- Includes are project-root-relative: `#include "vox/core/Log.h"`.
- `PascalCase` types/methods, `m_camelCase` members, `s_camelCase` statics.
- Engine logs via `VOX_*` macros, game logs via `GAME_*`.
