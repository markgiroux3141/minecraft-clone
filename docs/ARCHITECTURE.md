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

## game/ module layout

Game code (`namespace vc`) is grouped by responsibility — do not drop new files
at the root of `game/src/` (only the app shell `GameApp`/`Main` belong there).
See the "Where new code goes" table in `CLAUDE.md` for the rule.

```
game/src/
  GameApp, Main, InputState   app shell + its input-edge state
  entity/    Entity (Body + LivingAnim bases), Player, Mob, HumanoidModel, PigModel
  item/      Item, Inventory, Crafting
  render/    ViewModel, Particles      (game-side render helpers)
  ui/        screens, HUD, widgets, block icons
  audio/     game-event → sound mapping
  world/     chunks, meshing, lighting, worldgen, blocks, block entities, save
```

Shared entity state (render-interpolated position, walk-cycle accumulators)
lives once on the `vc::Body` / `vc::LivingAnim` bases in `entity/Entity.h`;
`Mob`, `World::ItemEntity` and the debug mob compose them instead of
re-declaring the fields. (`World::FallingBlock` opts out — it is column-locked
1-D motion, not a free vec3 body.)

## God-object carve-down (in progress)

`GameApp` (~1.6k lines) and `World` (~1.8k lines) are the two files that accrete
one-off features because they orchestrate everything. They're being split into
subsystems incrementally — each extraction should land green and shrink the
parent. `scripts\check_sizes.ps1` tracks them as exempt-but-watched. Done /
planned, roughly in priority order:

- ✅ Input edge/repeat state → `vc::InputState` (`InputState.h`).
- ✅ Audio runtime reconciliation (furnace crackle loops, footstep distance,
  dig-sound pacing) → folded into `vc::GameSounds` (`audio/`): `ReconcileFurnaceLoops`/
  `StopAllFurnaceLoops`, `Reset/UpdateLocomotion`, `Reset/TickDigSound` own the voice
  map + footstep/dig state; GameApp just drives them with world refs.
- ⬜ Entity simulation (item entities, falling blocks, mob list ownership +
  tick) → a `vc::EntityManager` owned by `World`, so `World.cpp` keeps only
  voxel/chunk concerns.
- ⬜ Input *handling* (`HandleInput`, the break/place/use verbs) → an
  `InputController` that operates on Player/World, leaving `GameApp` as pure
  wiring.
- ⬜ Frame telemetry (counters + title-bar string) → a small `FrameStats`/HUD
  helper once the title string stops mixing in world/clock state.

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
- **M22 — Audio** ✅: `vox::AudioEngine` (miniaudio high-level engine +
  stb_vorbis pre-decode to PCM, 3D spatialized one-shots, looping voices,
  Master/Sfx/Music/Ambient buses, generation-tagged voice handles) behind
  a PIMPL facade; `BlockDef::soundType` (vanilla StepSound classes) driving
  per-material dig/break/place/footstep sounds, item-pickup pop, water
  splash, per-lit-furnace crackle loop (reconciled each frame, World stays
  audio-free via `ForEachLitFurnace`), dark-cave ambient, and sparse
  background music (decoded on demand, one track at a time). Real 1.12
  `.ogg`s import from the MCP source's hashed object store
  (`mcp940/jars/assets`, resolved via `indexes/1.12.json`) into gitignored
  `assets/mc/sounds/` (zero distribution); a clean clone loads invalid
  handles and runs silent.
- **M23 — Model-block stream** ✅: a second per-cell chunk vertex stream
  (`ChunkMesh::modelVertices`, 24 B float-position `ModelVertex`, own
  `vox::MeshPool` + `model_block.vert` sharing `chunk.frag`) for non-cube
  geometry, so fractional blocks no longer steal bits from the packed cubic
  vertex. `BlockDef::model` is a list of vanilla-style `ModelBox` elements
  (from/to in 1/16 units + per-face tile/UV); the mesher emits them via
  `EmitModelBox`. The torch is the first client (its old packed-inset hack
  removed), with its flame cap now at the exact 10/16 height. Foreseen next
  clients: slabs/stairs/fences/panes.
- **M24 — Block orientation / facing** ✅: a per-cell metadata layer
  (`Chunk::m_meta`, parallel `std::array<uint8_t, kVolume>`, COW like the
  ids) carries block-specific orientation. `World::SetBlock(pos, id, meta)`;
  the chunk blob format gained meta-bearing variants (2/3) written only when
  a cell is oriented (unoriented chunks stay bit-identical to pre-M24). The
  mesher reorients horizontalFacing cube fronts (furnace/table front points
  at the placer) and applies a cell-local transform to model boxes (wall
  torches tilt out from their wall). `namespace facing` (Block.h) holds the
  helpers. First clients: furnace, crafting table, torch — reused by
  slabs/stairs later.
- **M25 — Deeper world** ✅: `kWorldHeightChunks` 4 → 8 (64 → 128 blocks)
  for vanilla underground depth. The structural code all derives from the
  constant (LOD height = /2, lighting, snapshots, culling, save); the
  worldgen tuning was rebased — `kSeaLevel` 14 → 63, heightmap lifted so
  flats sit ~y68 above the beach band (extreme hills ~y102), `kSnowLine`
  → 90, ore bands → vanilla (coal y2..124, iron y2..62), cave start heights
  doubled, spawn raised to y104. New world required (worldgen changed).
- **M26 — Lava** ✅: lava as a real emissive fluid. `World::UpdateLiquid`
  generalized from water-only to a per-liquid param set (decay, slope-find,
  infinite-source, tick rate) keyed off a new `BlockDef::liquidSource`
  family tag; lava spreads ~3 blocks (decay 2) on a 30-tick clock, no
  infinite source. Lava/water mixing (`CheckLavaMixing`, ported from
  `BlockLiquid`): source+water → obsidian (new block, iron-pick harvest),
  strong flow+water → cobblestone, lava-down onto water → stone. Worldgen
  fills carved cells below y10 with lava (vanilla `MapGenCaves.digBlock`).
  Orange eye tint + dense fog, molasses lava swimming, no damage (no health
  system). Follow-up: BUCKETS (empty/water/lava items, 3-iron recipe, RMB
  fill a source / dump a source via a liquid-aware raycast). New world
  required (worldgen changed); re-import MC assets (new atlas layers 64-68).
- **M27 — Water generation** 🚧: restore surface water lost after M25 made
  the world deeper (every biome had positive base height, so terrain never
  dipped below sea level). PART A — OCEANS ✅: a low-frequency
  "continentalness" noise field (`TerrainGen.cpp`) flips columns to new
  `Ocean`/`DeepOcean` biomes with NEGATIVE base height (-1.0/-1.8, vanilla
  values); the existing param blend + beach band + M11 water fill turn that
  into sloping sandy coastlines and basins (~25% ocean, deep floors ~y31)
  with no render changes. PART B — LAKES ✅: `WorldGenLakes` ported as a
  deterministic populate step (`LakeGen`, before tree/plant decoration).
  Self-sealing water/lava ponds dug into flat ground; leaks/seams are
  impossible by construction (chunk-aligned footprint with an interior margin,
  anchored below the lowest footprint surface, fit to one vertical chunk, and
  cave-rejected via the carve mask). A `LakeMask` vetoes trees/plants in the
  pond. New world required.
- **M28 — Slabs & stairs** 🚧: stone/cobble/plank/sandstone slabs + straight
  stairs (no auto-corner shaping yet), reusing each material's textures. The
  shape is built per-cell in the mesher from M24 meta (slab half; stair facing
  + upside-down) into M23's float model stream with auto-UVs + boundary-face
  culling. New `World::CollisionBoxesAt` gives per-cell partial AABBs and
  `Player::MoveAxis` resolves against them; `TickWalk` gained a 0.6 auto-step
  so you walk up them without jumping. Placement reads the clicked half/look
  direction (`RaycastHit` now carries the hit point); two matching slabs merge
  into the full block. Crafted (3-in-a-row → 6 slabs; stair triangle → 4).
- **M29 — 3D block icons** ✅: inventory/HUD block icons render as the vanilla
  3D iso model (`[30,225,0]` / scale 0.625 / per-face shade) instead of flat
  tiles, baked once into an offscreen sheet via the new `vox::Framebuffer` and
  blitted through the existing 2D `DrawImage` path. Items/plants stay flat
  (vanilla "generated" sprites). New `game/src/ui/BlockIcons` + `block_icon`
  shader; re-bakes only on a GUI-scale change.
- **M30 — Survival IV: health, damage & hunger** ✅: player health (10 hearts)
  + hunger/saturation on `Player`, ticked at 20 TPS. Damage sources wired into
  the existing tick (fall/lava/fire/cactus/drowning/void — the "no damage —
  no health system" hooks already stubbed throughout), vanilla regen gated on
  hunger, starvation, death → respawn. HUD heart/food/bubble rows via the
  existing icon path. No new renderer, no mobs; health/hunger persisted.
- **M31 — Entity model renderer** ✅: `vox::BoxModel` — a jointed multi-cuboid
  "box model" renderer (vanilla `ModelBase`/`ModelRenderer`) in the engine —
  named parts with per-part pivot/rotation, one skin texture, classic per-box
  UV unwrap, hierarchical transforms, render-interpolated. Ships with the Steve
  humanoid (`vc::HumanoidModel`, a verbatim `ModelBiped.setRotationAngles` port)
  + walk/idle animation on a debug entity (press G). THE shared dependency of
  mobs and the in-UI player doll.
- **M32 — Mobs (AI, spawning, combat)** ✅: a `Mob` entity (game/src/world/Mob.h)
  over M30/M31 — health + an AABB collided against blocks + an M31 skinned model.
  One passive (pig, quadruped `PigModel`) + one hostile (zombie, the biped reused
  with the zombie skin/pose); wander/idle vs target-path-to-player + melee;
  light/cap spawn rules + despawn; mob attacks deal M30 damage with directional
  knockback, player LMB melee + knockback + red hurt-flash, mob drops (porkchop/
  rotten flesh) reuse M18 item entities; persisted in a mobs.dat sidecar.
- **M33 — Armor & in-UI player doll** 📋: equippable armor (4 slots × tier)
  with M19 durability + damage reduction in M30's path; activate the inert
  inventory armor slots; render the player character (M31 model baked through
  the M29 framebuffer→UI path) in the inventory, worn armor shown.
- **Backlog**: stair auto-corner shapes (inner/outer), per-face light sampling
  for model blocks (slab faces are flat-lit), tight selection/raycast box for
  partial blocks, full 256-tall world (wants empty-air-chunk culling
  first), tall-grass wheat seeds (1/8 chance, BlockTallGrass — pairs with
  farming), animated water, stars, world-list scrolling, settings screen,
  vanilla's 14/16 cactus inset, 3D-extruded item sprites in hand (flat
  quad for now), view bobbing.
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
