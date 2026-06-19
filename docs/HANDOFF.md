# Session Handoff — Voxcraft

Updated: 2026-06-19, written for a FRESH CONTEXT. M39 (SPIDER — the first
roadmap-step-6 BESPOKE MOVER, wall-climbing) is now DONE and USER-VERIFIED
("that works") — see the "M39" section for the full design (debug spawn key
`L` = spider). The user decided to PARK the remaining bespoke movers
(slime/enderman/bats) for now and tackle REDSTONE next — see the "Redstone
roadmap" section near the end for the scoped milestone breakdown. It adds `MobType::Spider` (a wide
1.4×0.9 melee crawler, 16 hp), a `SpiderModel` (port of vanilla ModelSpider —
head/neck/body + 8 phase-paired legs), string + spider-eye drops (atlas tiles
121/122 → atlas is 123 layers now), and the bespoke MOVEMENT: two data-driven
`MobDef` flags — `canClimb` (vanilla `PathNavigateClimber`/`isOnLadder`: pinned
against a wall it wants to cross → ascends instead of stalling) and
`neutralInLight` (vanilla brightness give-up: only actually chases when it's dark
where it stands, otherwise wanders). IMPORTANT after pulling: re-run
`import_mc_assets.py` — M39 added the spider skin (`entity/spider/spider.png`),
the `mob/spider/` sound family (say/step/death; 225 sound files now), and 2 atlas
item tiles. The NEXT milestone is open — more roadmap-step-6 bespoke movers (slime
split / enderman teleport / bats) or a FARMING milestone (would give M38's
wheat/carrot/seeds a real in-world source).

M38 (BREEDING + BABY ANIMALS —
roadmap step 5's PAYOFF) is now DONE and USER-VERIFIED ("that works") — see
the "M38" section for the full design. It adds three
breeding items (wheat / carrot / seeds, plain non-food sprites in the creative
palette), an RMB-feed interaction (`GameApp::TryFeedMob`) that puts adult animals
into love mode, a love-seeking `TickMobs` branch that spawns a half-scale baby
when two in-love adults of the same species meet, and baby grow-up (the M34
`modelScale` hook is the baby-size lever — uniform 0.5). baby + grow timer
PERSIST in mobs.dat. Debug spawn key `H` = a baby cow. IMPORTANT after pulling:
re-run `import_mc_assets.py` — M38 added 3 atlas item tiles (wheat 118 / carrot
119 / seeds 120, atlas is 121 layers now), already imported on this machine. The
NEXT milestone is open — see "Mob & enemy roadmap" step 6 (bespoke movers:
spider climb / slime split / enderman teleport) or a FARMING milestone (would
give wheat/carrot/seeds a real in-world source instead of the creative palette).

M37 (FOOD / EATING — vanilla
`ItemFood`, roadmap step 5's prerequisite) is now DONE and USER-VERIFIED (eating
+ furnace cooking both confirmed working) — see the "M37" section for the full
design.
It made the existing M32/M34 meat drops (raw porkchop/beef/mutton/chicken +
rotten flesh) edible with vanilla 1.12 values, added COOKING (the four raw meats
smelt in a furnace into cooked variants — cooked porkchop/steak/mutton/chicken,
atlas tiles 114–117), and wired RMB-HOLD-to-eat: a 32-tick (1.6 s) eat with a
chewing crunch + crumb particles, a burp on the last bite, the vanilla eat-shake
view-model pose, and `Player::Eat` feeding the M30 hunger/saturation. The eat
gate mirrors the M36 bow draw in `GameApp::HandleInput`. IMPORTANT after pulling:
re-run `import_mc_assets.py` — M37 added `random/eat1..3` + `random/burp` sounds
AND 4 atlas item tiles (cooked foods 114–117, atlas is 118 layers now),
already imported on this machine. The NEXT milestone is the
roadmap step 5 PAYOFF: BREEDING + BABY ANIMALS (the M34 `modelScale` hook is the
baby-size lever; feed-with-food → love mode → spawn a scaled-down child) — see
the "Mob & enemy roadmap" section.

M36 (PROJECTILE SYSTEM →
SKELETON + player bow/arrows, roadmap step 4) is now DONE and USER-VERIFIED
(skeleton renders + aims + holds a visible bow, arrows fly/stick/hurt, the player
bow draws + fires, the skeleton is catchable) — see the "M36" section for the
full design (debug spawn key J = skeleton; grab a bow + arrows from the creative
palette, RMB-hold to draw). It built the shared arrow entity
(`EntityManager::Arrow` in game/src/entity/Projectile.cpp) and drives it from
both a Skeleton mob's ranged bow AI and the player's bow. POST-VERIFY FIXES
(2026-06-18, from the user's first look): the skeleton skin is 64x32 not 64x64
(it was rendering as a grey blob — see the M36 section); the skeleton now holds a
visible bow in its hand (`HeldBowModel` at the right-arm joint via
`HumanoidModel::RightArmTransform`); and the kiting was retuned — arrows apply a
much smaller knockback than melee (vanilla un-enchanted arrows barely shove) and
the skeleton only backpedals when the player is very close (~30% of bow range),
at reduced speed, so a walking player can run it down. The NEXT milestone is
roadmap step 5: FOOD/EATING → breeding + babies — see "Mob & enemy roadmap" +
the "Backlog" eating note. M35 (EXPLOSION SYSTEM →
CREEPER + TNT, roadmap step 3) is now DONE and USER-VERIFIED ("it all works") —
see the "M35" section for the full design. It built the shared
`EntityManager::Explode` routine and drives it from both a Creeper mob and a
primed-TNT entity. M34 (PASSIVE
ROSTER — cow +
sheep + chicken) is CODE COMPLETE awaiting user verification — see the "M34"
section for the full design + the user test checklist (debug spawn keys V = cow,
N = sheep, M = chicken). It built on the M32 mob
framework and did the two roadmap generalizations (data-driven spawning + a
model-scale hook). M0-M20, M22 (AUDIO), and
M23 (MODEL-BLOCK STREAM) are all done and USER-VERIFIED (M22: "beautiful,
it all works"; M23: "everything looks perfect and the torch cap is where
it should be"). M21 (ORES + FURNACE/SMELTING + torches), M24 (BLOCK
ORIENTATION / FACING), M25 (DEEPER WORLD — 128 tall, vanilla
underground depth), and M26 (LAVA) are CODE COMPLETE awaiting explicit user
verification — see their sections for what was built and what to test (M25
and M26 need a NEW WORLD). M27 (WATER GENERATION — oceans + lakes) is now
CODE COMPLETE too (oceans user-verified "looks good"; lakes awaiting
verification) — see the M27 section. M28 (SLABS & STAIRS) is now CODE
COMPLETE awaiting verification — stone/cobble/plank/sandstone slabs + straight
stairs, with partial collision + a 0.6 auto-step so you walk up them; see the
M28 section for the full design and the user test checklist. M29 (3D BLOCK
ICONS) is now CODE COMPLETE and user-verified ("looks good") — inventory/HUD
block icons render as the vanilla 3D iso model instead of flat tiles; see the
M29 section. The next arc was SCOPED with the user (2026-06-15): survival &
mobs across M30–M33 (health → entity-model renderer → mobs → armor + player
doll in the UI), landing IN ORDER one at a time — see the "Planned arc —
survival & mobs" section for the full breakdown and the why-this-order. M30
(HEALTH, DAMAGE & HUNGER) is now DONE and USER-VERIFIED, including the
hurt-feedback (damage sound + camera tilt) and the real first-person fire
overlay follow-ups — see the M30 section for what was built. M31 (ENTITY MODEL
RENDERER — the `vox::BoxModel` jointed box-model renderer + a Steve debug entity
on the G key) is now DONE and USER-VERIFIED ("works perfect") — see the M31
section for the design. M32 (MOBS — AI, spawning, combat) is now CODE COMPLETE
awaiting user verification — one passive (pig) + one hostile (zombie) with AI,
natural light/cap-gated spawning, two-way melee combat (player LMB + knockback,
zombie melee → directional player knockback), mob item drops, and mobs.dat
persistence; built on M31's renderer + M30's vitals + the FallingBlock/ItemEntity
pattern. See the M32 section for the design + the user test checklist (debug
spawn keys B = pig, C = zombie). M33 (ARMOR + THE INVENTORY PLAYER DOLL) is now
DONE and USER-VERIFIED ("the character model is facing in the right direction,
armor works, everything looks good") — full vanilla armor set (leather/chainmail/
iron/gold/diamond × helmet/chest/legs/boots = 20 items), vanilla 1.12 CombatRules
damage reduction + durability wear, the four inventory armor slots (equip on
click, type-gated), and a 3D player doll baked into the inventory panel showing
worn armor. See the M33 section for the design. This closes the M30–M33 survival
& mobs arc; the next milestone (mob roster / explosion / projectile systems) is
scoped in the "Mob & enemy roadmap" section, in a fresh context. NOTE: after
pulling on this machine, re-run `import_mc_assets.py` (M33 added armor icons,
model-layer textures, and empty-slot sprites).
Read alongside `ARCHITECTURE.md` (layering rules, roadmap) and `CLAUDE.md`
(build commands, conventions).

IMPORTANT after pulling on this machine: re-run `python
scripts/import_mc_assets.py` — M26 added two atlas layers (lava 64,
obsidian 65), so the gitignored `assets/mc/atlas.png` overlay is stale until
re-imported (the committed placeholder atlas is already regenerated). M30's
hurt-feedback follow-up also added a new sound family (`damage/`) and the fire
overlay texture (`blocks/fire_layer_1.png` → `mc/textures/fire_layer_1.png`), so
the same re-import pulls `damage/hit{1,2,3}.ogg` (player hurt noise) and the
on-fire flames. M32 added two more atlas item sprites (raw porkchop 69, rotten
flesh 70), the pig/zombie entity skins (`entity/pig/pig.png`,
`entity/zombie/zombie.png`), and two sound families (`mob/pig/`, `mob/zombie/`),
so re-importing is needed for mobs to render + sound (161 sound files now). A
clean clone with no overlay draws no mob and is silent, like the debug Steve.
M36 added 6 atlas item tiles (bow 108, bow_pulling_0..2 109–111, arrow 112,
bone 113 → atlas is 114 layers now), the skeleton skin
(`entity/skeleton/skeleton.png`), the arrow projectile texture
(`entity/projectiles/arrow.png`), the `mob/skeleton/` sound family, and the
`random/bow.ogg` shoot twang — so the same re-import pulls those (a clean clone
with no overlay draws no skeleton / no arrow and is silent for the bow).

IMPORTANT RESOURCE: the user has Minecraft's Java source (MCP 9.40 =
1.12) at `D:\Minecraft source code` — look up exact game dynamics there
(e.g. block/BlockDynamicLiquid.java drove the M13 water rules) instead
of recalling them. Its bundled assets (textures/models/lang under
mcp940/src/minecraft/assets/) are fair game: personal use only, zero
distribution (user's explicit call).

## Where the project stands

M0–M16 are done and user-verified:
- Engine (`engine/src/vox/`, namespace `vox`): fixed-timestep app loop
  (20 TPS + interpolated render), GLFW window/input (incl. cursor capture),
  GL 4.6 renderer facade with DSA abstractions (Shader, Buffer/VertexArray,
  Texture2D, Texture2DArray, PerspectiveCamera, Frustum, DrawIndexed +
  DrawLines), spdlog logging, asset-root resolution (`vox::assets`),
  `vox::ThreadPool` (std::jthread workers, FIFO `move_only_function` queue;
  queued-but-unstarted jobs are discarded on shutdown).
- Game (`game/src/`, namespace `vc`): data-driven `BlockRegistry` (air=0,
  stone, dirt, grass), 16^3 `Chunk` (YZX order), greedy `ChunkMesher` with
  baked AO, threaded `World` streaming manager with versioned edits (all
  below), FastNoiseLite heightmap `TerrainGenerator` (seed 1337),
  `Player` (walk physics + fly mode + break/place).
- Block textures: GL texture array (NOT an atlas — no mip bleeding, UVs can
  tile for greedy meshing). Layers generated by `scripts/gen_textures.py`
  into `assets/textures/atlas.png`; layer order must match
  `blocks::RegisterDefaults()` in `game/src/world/Block.cpp`.

## Controls

Starts on the title screen (cursor free): click a world / New World /
Quit. In game the cursor is captured: WASD move, mouse look, Space jump
(walk) or rise (fly), LeftShift sink (fly), LeftControl sprint/boost,
F toggles walk/fly, O toggles occlusion culling (debug), T fast-forwards
world time (debug), G spawns/despawns the debug Steve (M31), B/C spawn a
debug pig/zombie ~3 blocks ahead (M32), V/N/M spawn a debug cow/sheep/chicken
(M34), K spawns a debug creeper (M35), J spawns a debug skeleton (M36), H spawns
a debug baby cow (M38), L spawns a debug spider (M39 — climbs walls, only
aggressive in the dark), RMB-hold
with a bow drawn from the hand charges + fires an arrow on release (M36, consumes
an Arrow; full draw hits harder), RMB-hold a FOOD item (raw meat / rotten flesh)
eats it over ~1.6 s when hunger isn't full (M37, restores hunger/saturation,
consumes one), RMB an animal with its breed item in hand (wheat=cow/sheep,
carrot=pig, seeds=chicken) feeds it (M38 — adults enter love mode + pair up into
a baby, babies grow up faster; consumes one), LMB hold-to-break in walk (crack overlay, drops pop
out as item entities and vacuum into the inventory; tools dig faster,
stone needs a pickaxe to drop) / instant pop in fly (creative-style, no
drops), RMB place (consumes one) or use a crafting table (opens its
3x3), Q tosses one from the hand, 1..9 select the hotbar slot, E
opens/closes the inventory screen (slots + 2x2 craft grid + a creative
palette of all blocks AND items; clicking outside the panels throws the
carried stack; new worlds start with the legacy kit:
stone/dirt/grass/glowstone/sand/log/leaves/water x64, slot 9 empty).
In water: W swims toward the look
direction, Space swims up (breach kick at the surface). Esc pauses
(Resume / Save & Quit to Title) — quitting the app is the title screen's
Quit button or the window X. Default spawn (8.5, 104, 8.5) — above the
M25 128-tall terrain, so a fresh world drops you onto the surface; a world with
saved player state restores position/look/mode instead.

## M4 threaded pipeline + M6 versioned edits (how it works)

- `World` owns a `vox::ThreadPool`; generation and meshing run as jobs.
  Workers produce CPU-side data only (`shared_ptr<const Chunk>`,
  `ChunkMesh`) and push it to mutex-guarded completion vectors; `Update()`
  drains them on the main thread, which exclusively owns the chunk map and
  all GL calls.
- Block data is COPY-ON-WRITE: `ChunkEntry::blocks` is a
  `shared_ptr<const Chunk>` that is *replaced* (never mutated) by
  `World::SetBlock`, so worker snapshots stay valid without locks.
  Meshing jobs take a `ChunkSnapshot` (shared_ptrs to the full 3x3x3
  neighborhood — AO needs edge/corner chunks); meshing gates on all 26
  neighbors having data.
- Mesh freshness is VERSIONED, not a state machine: `dataVersion` (bumped
  per edit; 1 on generation), `meshedVersion` (what the uploaded mesh was
  built from), `meshingVersion` (what the in-flight job targets). A chunk
  needs meshing when `meshedVersion != dataVersion`; results are accepted
  only when `result.version == dataVersion`, else dropped and the scan
  resubmits. The old mesh keeps rendering until the new one lands (no
  flicker). Border edits also bump the affected neighbors' `dataVersion`
  (face culling + AO reach one block across the seam) — see
  `World::SetBlock`'s offset-mask loop.
- Job submission is capped at `workers * 4` in flight; mesh jobs are
  submitted before gen jobs, both nearest-first. View radius 12 (data ring
  13 → 2916 columns). World height is 8 chunks since M25 (128 blocks;
  surface ~y68, see the M25 section) — double the chunk count of the old
  4-tall world, so RAM/streaming roughly doubled.
- `World` member order matters: the pool is declared LAST so its
  destructor joins workers before the queues/map/generator die.

## M5 greedy meshing + AO (how it works)

All in `ChunkMesher.cpp`, pure functions of the snapshot:
- Worker flattens the snapshot into a padded 18^3 volume (ids + opacity),
  ~18 KB stack. Per face direction (FaceBasis table: u/v axes chosen so
  cross(u,v) = normal → CCW winding; cull face is ON), per 16x16 slice:
  mask keyed on (texture layer, 4 corner AO values) → greedy rectangle
  merge. Identical-key merging keeps AO gradients continuous.
- AO = classic 3-neighbor corner term, baked as 0..1 in `ChunkVertex::ao`;
  quads split along the brighter diagonal (no dark-cross artifact);
  `chunk.frag` applies `mix(0.4, 1.0, v_ao)`. UVs span (w, h) tiles
  (texture array wraps with GL_REPEAT). ~0.39M tris at radius 12 (~10x
  less than culled). Only opaque blocks are meshed; transparent types
  (water, leaves) need their own pass when they exist.

## M6 player (how it works)

- `Player` (game/src/Player.h/.cpp, replaced FlyCamera): feet-center
  position, AABB 0.6 x 1.8, eye 1.62. Simulated in `OnTick` (20 TPS) with
  prev/current positions; `Player::OnRender(alpha)` interpolates the eye
  and applies per-frame mouse look (look is per-frame on purpose).
- Walk: axis-separated AABB collision (Y then X then Z), clamp + zero
  velocity per axis, `kSkin` epsilon gap; gravity 32, jump 9, walk 4.3
  (x1.6 sprint). Physics freezes until the chunk underfoot has data, so
  the player never falls through generating terrain. Fly: noclip.
- Targeting: `World::RaycastBlocks` (Amanatides & Woo DDA, reach 5) from
  the eye; hit block drawn as an inflated wireframe cube
  (`Renderer::DrawLines` + outline.vert/frag — outline is near-black,
  hard to see against dirt; fine against grass/stone).
- Edits: GameApp acts on press then repeats every 0.25 s while held.
  Place cell = hit block + face normal, rejected if solid or overlapping
  the player AABB.

## How to verify a change

```powershell
.\scripts\build.ps1        # or the raw cmake commands in CLAUDE.md
.\scripts\build.ps1 -Run
```
From an agent sandbox: build, launch the exe, sleep a few seconds,
screenshot the primary display, and read the image. Title bar shows
fps/tps/eye-pos/mode/hand/chunk/job/triangle stats; stdout shows the log.
Input can be verified by injecting events (user32 `mouse_event` /
`keybd_event` via Add-Type after `SetForegroundWindow`) — used to verify
M6 break/place/fly. Exercise clean shutdown (`CloseMainWindow()`, expect
"shutting down" in the log) — it covers the ThreadPool join path.

## Known issues / decisions in flight

- One-time NVIDIA warning at startup: "Vertex shader in program 3 is being
  recompiled based on GL state" — harmless, not yet investigated.
- (Fixed in M12) GPU mesh uploads are budgeted at 2 MB/frame — overflow
  results defer to the next frame (safe: acceptance is versioned, stale
  deferred results drop). ChunkVertex is two packed uint32s (8 B); the
  pool sits around ~25 MB at radius 12 + LOD.
- Block outline is light gray since M10 — fine on dirt/stone/grass, may
  wash out on pale blocks (glowstone); GL core can't draw thick lines,
  so a real fix means quad-based outlines.
- Title screen world list shows at most 6 worlds (no scrolling yet).
- Debug-build fps dips hard during big water-flow bursts (remesh + light
  recompute churn; worker files are /O2 even in Debug since M13, release
  is fine — user accepts). Next lever if needed: batch/coalesce light
  recomputes during flow bursts.
- Remote: https://github.com/markgiroux3141/minecraft-clone.git (branch
  `main`). Commit + push at milestone boundaries.

## M7 lighting (how it works)

- `ChunkLight` (world/Light.h): one byte per block, sky<<4 | block, both
  0..15. COW shared_ptr on ChunkEntry, exactly like blocks. World height
  constant moved to Light.h (`kWorldHeightChunks`) so the light engine and
  mesher don't depend on World.
- `LightEngine::ComputeColumn` (worker job): takes the 3x3 column
  neighborhood of block data, builds a 48x64x48 volume, column-scans
  direct sky (15 straight down until opaque), then FIFO BFS spreads
  sky and block light, attenuating 1/step through transparent cells.
  BFS range is 15, so 3x3 columns provably covers everything that can
  influence the center column; only the center column is extracted.
  Emissive opaque blocks (glowstone, emission=15 in BlockDef) seed block
  light but don't conduct.
- Per-column versioning in `ColumnEntry` (dirtySeq/litSeq/lightingSeq)
  mirrors mesh versioning. SetBlock dirties the 3x3 columns around the
  edit; when a light job lands, sections that actually changed (memcmp)
  replace the chunk's light and bump dataVersion of the 3x3x3 chunks
  around it (their meshes sampled that light). Unchanged sections don't
  cascade — that's what keeps edit-driven remeshing bounded.
- Radius layering: generate at view+2, light at view+1, mesh at view —
  each stage needs a 3x3 neighborhood of the previous, and this is what
  prevents gating deadlock at the stream edge. Mesh gating
  (`NeighborsReady`) now requires blocks AND light across the 3x3x3.
- Mesher: padded volume carries light; per-vertex smooth light averages
  the same 4 corner cells as AO (transparent cells only — opaque cells
  would double-darken corners on top of AO). Greedy merge key grew to
  uint64 (layer + 4xAO + 4xsky + 4xblock). Emissive blocks' own faces take
  max(sampled, emission). Above-world cells read sky 15 via
  `ChunkSnapshot::skyAbove`.
- `chunk.frag`: skylight carries the directional sun term, block light is
  warm (1.0, 0.85, 0.62) and omnidirectional, `max()` combine, AO on top,
  0.03 ambient floor.
- Edit-pipeline optimizations (commit dd73744, after user found a dark
  flash on break + fps drops while digging): (a) SetBlock patches the
  edited cell's light immediately (max of 6 neighbors - 1, sky 15 if the
  cell above has it, emission for placed emitters) so the instant remesh
  isn't pitch black while the flood fill catches up; (b) column dirtying
  is range-checked (Manhattan reach <= 15 — center-of-chunk edits skip
  diagonal columns); (c) light results compare BORDER SLABS old-vs-new
  and only bump neighbor meshes that can see a changed cell (neighbors
  sample at most one cell across the seam). All three live in World.cpp.
- Known tuning points: light is linear in level (Minecraft uses an
  exponential-ish curve — revisit if mid-levels look too dark); fps dips
  to ~12 during the initial streaming burst in debug builds (GPU upload
  bursts on the main thread — budget uploads per frame if it ever
  matters; it recovers to 60 once pending hits 0 and the user is fine
  with it).

## M8 persistence (how it works)

- `WorldSave` (world/WorldSave.h/.cpp): main-thread-only store of
  RLE-compressed blobs for EDITED chunks only (everything else
  regenerates from the seed). The whole store loads into memory at
  construction and files are never read again, so flushes can't race
  reads. Format doc is at the top of WorldSave.h: `saves/world/level.dat`
  (text: format version + seed — manifest seed wins over the compiled-in
  default) and `r.<rx>.<rz>.vxr` region files (32x32 chunk columns,
  header + coord/size index + blobs, rewritten atomically via temp +
  rename). Save dir is `assets/`'s sibling `saves/world/` (gitignored),
  resolved in GameApp::OnInit.
- World integration: `ChunkEntry::edited` set by SetBlock;
  `SubmitGenerate` decodes a blob copy on a worker instead of generating
  when the store has the chunk (corrupt blob → log + regenerate). Edited
  chunks are Put on unload, by a 30 s autosave sweep in Update, and in
  ~World (runs before the pool member's destructor joins workers, which
  is fine — workers never touch the map or the store). `Flush(false)`
  runs every Update but debounces disk writes to one pass per 3 s;
  ~World forces it.
- `savetest` target (game/tests/SaveTest.cpp): standalone round-trip
  regression test for the format — run `build/<config>/bin/savetest.exe`
  after touching serialization (it covers negative/multi-region coords,
  manifest seed precedence, overwrite, corrupt-blob rejection).
- Known limits (fine for now): a chunk edited back to exactly its
  generated contents stays in the save forever; the store keeps every
  saved blob in RAM (tiny while edits are sparse — revisit if saves get
  huge); single hardcoded world ("saves/world"), no save UI.

## M9 rendering at scale (how it works)

Stage 1 — multi-draw indirect (`vox::MeshPool`, engine/renderer):
- All chunk meshes live in ONE growable GL vertex buffer (first-fit free
  list, coalescing on Free, grows by copy — watch the "MeshPool: growing"
  log line). Each frame, World hands the pool a list of
  {handle, vec4(originXYZ, scale)} items and the pool issues a single
  glMultiDrawElementsIndirect; the shader reads the vec4 from an SSBO
  (binding 0) indexed by gl_DrawID. chunk.vert has no u_model anymore.
- Chunk meshes carry NO index data: the mesher emits quads under one
  shared pattern {0,1,2,2,3,0} (the pool repeats it with per-group
  offsets). The AO diagonal flip is a cyclic rotation of the vertex
  emission order in EmitQuad — same triangles, winding preserved.
- ChunkEntry holds a MeshHandle; World::UploadMesh(handle, indexCount,
  mesh) frees-then-allocates. Unload paths must Free handles (Update's
  erase loops do; the pool dies with World otherwise).

Stage 2 — occlusion culling (cave culling):
- Mesh jobs flood-fill the chunk's non-opaque cells and record which face
  pairs connect: 15 bits (world/Visibility.h) on ChunkMesh/ChunkEntry.
- World::CollectVisibleChunks BFS-walks the chunk grid from the eye
  (visit-stamped grid, queue scratch reused), descending only through
  connected face pairs and never stepping back toward the camera
  (dirMask). Frustum gates draw-list inserts, NOT traversal (frustum
  pruning of traversal would false-cull chunks reachable only through
  off-screen neighbors). Unmeshed chunks traverse permissively. Above/
  below-world cameras seed every column's top/bottom chunk. BFS order is
  ~front-to-back (early-z friendly). O key toggles for comparison.

Stage 3 — LOD shell:
- LOD chunk = 16^3 cells of 2^3 blocks (32^3 world blocks; 2 LOD chunks
  of world height). LOD gen jobs run the REAL generator over the 16
  underlying chunks and downsample 2x2x2 -> cell: most common non-air id,
  ties to the topmost (keeps grass tops); any solid source block makes
  the cell solid, so LOD is a strict SUPERSET of real terrain — that
  superset rule is what keeps the detail/LOD seam hole-free (LOD bulges
  <= 1 block instead of gapping).
- LOD meshing reuses ChunkMesher with a shared full-bright skylight,
  gated on the 3x3 LOD-column neighborhood; meshes go in the same pool,
  drawn with scale 2 in the per-draw vec4. Built once — no edits, no
  relight at LOD (saved-chunk edits are NOT visible at LOD distance).
- Rings (World.h constants): draw/mesh where far-corner > kViewRadius
  and near-corner <= kLodRadius (32); data ring one LOD column wider both
  ways. LOD work is lowest priority, nearest first, same in-flight cap.
- Handover: a LOD column inside the detail radius keeps drawing until
  every real chunk under it is meshed (DetailMeshedUnder) — no hole ring
  chasing a fast player.

## M10 UI/menus (how it works)

- `vox::UiRenderer` (engine/renderer): batched 2D overlay — solid rects,
  monospace bitmap text, and single texture-array layers (block icons),
  one blended draw call per End() (auto-flush at 4096 quads). Pixel
  coords, origin top-left; draws with depth off + blend on, restores GL
  state after. The shader is EMBEDDED in UiRenderer.cpp (vertex layout
  and sampler contract live with the batcher; per-vertex mode selects
  solid/font/atlas, textureLod avoids derivative issues in the branches).
- Font: `scripts/gen_font.py` (Pillow) bakes Consolas 16 px, thresholded
  to 1-bit, into `assets/fonts/ascii.png` — ASCII 32..127 in a 16x6 grid
  of 9x17 cells. Game passes it to SetFont; glyph size derives from the
  image. Texture loads flipped (GL bottom-left origin) — glyph/icon UV
  math in UiRenderer/DrawAtlasTile accounts for it.
- Game UI (game/src/ui/): `Hud` (crosshair + hotbar + name label),
  `PauseMenu`, `TitleScreen`, shared `Widgets` (UiButton/ShadowedText).
  All immediate-mode statics: Draw(ui, screen, mouse, clickEdge) returns
  what was clicked; GameApp owns all state. `GuiScale()` (Hud.h) is
  Minecraft's auto rule: largest integer scale fitting 320x240 (3 at
  1600x900); hotbar icons are 16 px tiles at integer scale (crisp).
- GameApp state machine: Title (no world, free cursor), Playing
  (captured), Paused (free cursor over live world). Esc toggles pause;
  while paused the player tick / mouse look / targeting / edits freeze
  but world streaming keeps running. Two guards worth keeping: Player
  look-delta tracking resets while look is disabled (no camera jerk on
  resume), and EnterWorld/SetPaused(false) set break/place wasDown +
  cooldowns so the menu click can't fall through into a block edit.
- World lifecycle: GameApp::EnterWorld creates vc::World on demand
  (saves/<name>); ExitToTitle persists player state, resets the
  unique_ptr (~World saves edited chunks + force-flushes), rescans the
  world list. New World picks the first free of world, world2, ... with
  a std::random_device seed (manifest persists it; existing manifests
  win over any default).
- Player state in the manifest: optional `player x y z yaw pitch fly`
  line in level.dat (WorldSave::Get/SetPlayerState; SetPlayerState
  rewrites the manifest immediately — quit-path only). Absent line =
  pre-M10 save = default spawn. savetest covers the round-trip and that
  seed/chunks survive the manifest rewrite.

## M11 gameplay depth (how it works)

- Blocks: sand/log/leaves/water appended after the M10 set — BlockIds are
  stored in save blobs, so registrations may only APPEND, and texture
  layer order must match scripts/gen_textures.py (atlas.png is RGBA now;
  water's alpha is 168). BlockDef grew a `liquid` flag. Leaves are
  opaque for now (cutout deferred).
- Terrain: beach band (sand surface) where height <= kSeaLevel+2; water
  fills air up to kSeaLevel (14, in TerrainGen.h). Trees: one candidate
  per 8x8-column cell (seeded hash gate at 45% + jittered position),
  classic oak shape, grass-only ground. Chunks enumerate all cells whose
  canopy (radius 2) could reach them and visit cells in a stable order,
  so every chunk independently regenerates identical trees; decoration
  only writes into air (plus dirt under the trunk). `gentest.exe` guards
  determinism, trunk contiguity across seams, and leaf anchoring — run
  it after touching TerrainGen.
- Water rendering: the mesher routes liquid faces into
  ChunkMesh::transparentVertices (mask key bit 57); liquid faces show
  only against non-opaque non-liquid neighbors. Chunks and LOD columns
  carry a second MeshPool handle (meshT); CollectVisibleChunks returns a
  second draw list sorted back-to-front by chunk center, drawn after
  opaque with blend on, depth write off, cull off (surface visible from
  below). chunk.frag passes texture alpha through. LOD downsample lets
  water win only all-liquid cells (solid-superset seam rule survives
  shorelines).
- Swimming (Player::TickWalk): in water, W follows the full look
  direction (SwimWishDir), Space swims straight up with a stronger
  breach kick when the head is out (climbs 1-block shores), gentle sink
  otherwise, 55% horizontal speed. Eye-in-water draws a blue UI tint on
  top of short blue shader fog.
- Day/night: GameApp::m_worldTime (ticks; 24000 = 20 min/day), advances
  only while Playing, persisted as an optional `time` line in level.dat
  (manifest reader is a tag loop now — player/time lines in any order).
  ComputeDayNight (GameApp.cpp) maps t (0 = sunrise 06:00) to sun
  direction, skylight scale (0.12 moonlight floor), and sky colors;
  sky.vert/.frag draw a fullscreen gradient dome + sun disc before the
  terrain (depth write off); chunk.frag takes u_sunDir/u_sunLight and
  distance fog (320..500 outside, 2..28 deep blue underwater). Title
  bar shows the in-world clock.
- Deferred for a future block-update milestone: water flow + falling
  sand (landed in M13), water-attenuated skylight + cutout leaves
  (landed in M16), lowered water surface (M13), moon (M14) / stars
  (still open).

## M12 perf polish (how it works)

- ChunkVertex = two uint32s (8 B; was 48 B of floats):
  data0 = x:5|y:5|z:5|normal:3|ao:2|sky:4|block:4 (bits 28..31 free)
  (positions are cell corners 0..16, normal indexes BlockFace order;
  bits 28..31 once held the M21-torch insets — torches moved to the M23
  float model stream and the bits are free again), data1 =
  u:5|v:5|layer:16|yoff:4 (UVs tile 0..16 across merged quads; yoff
  lowers y by N/9 — liquid surface drop only now). Packed in EmitQuad,
  decoded bitwise in chunk.vert (kNormals table); the MeshPool layout is
  two UInt attributes (VertexArray routes Int/UInt types through
  glVertexArrayAttribIFormat). gentest includes a pack/decode smoke test
  (lone stone + water block) — run it after touching the format.
- Upload budget (World::DrainCompletedJobs): detail and LOD mesh results
  share a 2 MB/frame upload cap; overflow carries to m_deferredMesh /
  m_deferredLodMesh and processes next frame (already counted out
  of m_jobsInFlight; meshingVersion/meshInFlight stay set while
  deferred, so nothing resubmits early; the first item each frame always
  processes so one huge mesh can't starve).
  M26 follow-up fix: detail meshes upload NEAREST-FIRST (the carried
  backlog + this frame's results are merged and sorted by chunk distance to
  the player, so DrainCompletedJobs now takes centerX/centerZ). Before this,
  a player edit's remesh queued BEHIND the whole streaming backlog during
  world load, so a broken block lingered ~0.5 s until the backlog drained
  (then was instant once settled — the user reported exactly this). Distance
  ordering makes the player's own chunk (distance 0) always win the budget.

## M13 block updates (how it works)

- Tick system: World::Tick (20 TPS from GameApp::OnTick, frozen while
  paused) drains a priority queue of scheduled updates (512/tick cap).
  Every SetBlock wakes the edited cell + 6 neighbors — liquids at 4-tick
  delay, everything else at 2 — so cascades sustain themselves.
- Falling sand: BlockDef::gravity; an unsupported gravity block detaches
  into World::FallingBlock (x/z fixed, float y, 18 b/s^2 accel, slow in
  water), drawn as a textured unit cube (block_entity shaders, per-face
  layers, light sampled at cell AND cell-above — the placed block's own
  cell reads 0) and re-placed on landing. Entity/mesh handover uses
  dataVersion/meshedVersion (syncCell/syncVersion on the entity): hidden
  at detach until the remesh drops the old block, lingering at landing
  until the remesh shows the new one — no z-fighting, no gap. ~World
  settles in-flight blocks so they persist.
- Water flow (World::UpdateLiquid, ported from the 1.12 source's
  BlockDynamicLiquid — see the resource note at the top): Water (id) is
  the source, WaterFlows[7] are appended flow-level ids (BlockDef::
  liquidLevel: 8 source, 7..1 flow), so storage/saves are untouched.
  Rules: flows re-derive their level each update (max neighbor - 1;
  falling neighbors count as 8; liquid above = 7) which is also how they
  recede; >= 2 adjacent sources over solid/source mint a new source
  (infinite water); pour-down beats sideways; flows over water never
  sheet (only sources do — ocean breaches flood at every depth, pillar
  water pours down the sides); sideways spread is slope-seeking (only
  toward the drop(s) nearest within 4 blocks, SlopeDistance recursion);
  sources and waterfall landings sheet at strength 7, others level-1.
  Worldgen oceans are all-source and surrounded by sources/land = stable
  until disturbed. Swimming/eye-tint check BlockDef::liquid.
- Partial-height rendering: liquids skip greedy merging (EmitLiquidCell,
  per cell) — each cell-top corner gets a height in ninths (level for
  the max liquid cell around the corner, 9 if any is submerged → ocean
  interiors stay flush, surfaces slope continuously; sources sit at
  8/9). The drop is packed into 4 spare bits of data1 (bits 26..29) and
  subtracted in chunk.vert. Raycast still ignores liquids — remove water
  by placing a block into it or cutting the source.
- Not done (vs Minecraft): no flow-direction surface texture animation,
  no swimming current push, lava, bucket item.

## M14 real Minecraft assets (how it works)

- `scripts/import_mc_assets.py` (Pillow) reads the user's 1.12 source
  tree and writes into `assets/mc/` — GITIGNORED, because the repo is on
  a public GitHub remote and the user's rule is zero distribution. The
  game prefers the overlay via `PreferMcAsset()` (GameApp.cpp) or
  explicit existence checks; the committed placeholder assets keep a
  clean clone working. Re-run the script after pulling on this machine.
- Block atlas: same 10-layer strip contract as gen_textures.py
  (layer order = blocks::RegisterDefaults()). Baked at import: grass
  top/side-overlay x plains grass colormap tint (145,189,89), oak leaves
  x foliage tint (119,171,47) over an opaque dark backdrop (we render
  leaves opaque — vanilla "fast graphics" look), water = frame 0 of the
  16x512 animation strip (already blue + alpha).
- Font: real `font/ascii.png` (16x16 grid of 8x8 cells from char 0).
  UiRenderer::SetFont grew `proportional` — it reads the texture back
  and scans per-glyph inked width (advance = rightmost+2, empty cell =
  half cell), like MC's FontRenderer. MeasureText sums advances. UI text
  uses the full GUI scale with the 8px font (`UiTextScale()` in
  Widgets.h returns s-1 only for the 17px placeholder font).
- GUI: UiRenderer::DrawImage (mode 3, sampler unit 2) draws sprite-sheet
  sub-rects in image-pixel coords (flush on sheet switch). Hud draws the
  icons.png crosshair (MC's -7,-7 offset; plain alpha blend, not
  vanilla's invert — fine per user) and the widgets.png 182x22 hotbar +
  selection frame; UiButton draws the 200x20 button sprites (hover row +
  yellow label). The hotbar is 9 slots now (MC art is 9 wide): slots 1-8
  blocks, slot 9 empty hand (place is guarded on Air). `GuiTextures`
  (ui/Widgets.h) carries the optional sheets; null = procedural look.
- Sun/moon: celestial.vert/.frag draw eye-glued billboards (rotation-
  only view, distance 100, sun half-size 15, moon 10) right after the
  sky gradient, additive blend (`Renderer::SetBlend(BlendMode)` enum
  now: None/Alpha/Additive) — the sheets are opaque on black, so black
  adds nothing. Moon phase = day index % 8 into the 4x2 sheet; moon dir
  = -sunDir, faded out while the sun is up (DayNight::moonColor). The
  procedural sky.frag disc is disabled via u_disc when textured (halo
  kept); it remains the no-assets fallback.
- Night lighting fix (user: "insanely dark"): moonlight floor 0.12 →
  0.28, chunk/block_entity diffuse now follows DayNight::lightDir (sun
  by day, MOON at night — terrain keeps shape), and skylight is tinted
  by DayNight::skyTint (white day → blue 0.55/0.66/0.95 night). Day
  output is unchanged (floor + range still sum to 1.0).

## M15 caves + biomes + topology (how it works)

IMPORTANT for old saves: M15 changed worldgen (heights, surfaces, caves)
— unedited chunks in pre-M15 saves regenerate with the NEW rules and
will seam badly against previously edited/saved chunks. Test in a New
World.

- Caves (world/CaveGen.h/.cpp, called from TerrainGen between fill and
  trees — vanilla order): a port of 1.12 MapGenCaves, including an EXACT
  java.util.Random clone (the tunnel shapes live in its draw sequence —
  don't swap PRNGs). Per chunk, replay the 17x17 origin-chunk
  neighborhood (range 8), each origin seeded ocx*saltX ^ ocz*saltZ ^
  seed; tunnels are worm walks (sine-bulged radius, drift-damped
  yaw/pitch, midpoint branch into two perpendicular arms when radius>1,
  1-in-4 rooms with 0.5 vertical squash, the d9>-0.7 flat-floor quirk
  kept). Start heights scaled to our 64 world: nextInt(nextInt(54)+8).
  No lava floor yet (no lava block) — deep caves stay open; world keeps
  a 1-block floor (y>=1 clamp).
- Two deliberate departures from vanilla: (1) the ocean-breach test is
  ANALYTIC (worldgen water rule height<y<=sea over the UNCLIPPED sphere
  box) instead of scanning clipped chunk contents, so every chunk makes
  the same abort decision and carved air provably never touches
  worldgen water (gentest asserts it); (2) a CarveMask (chunk + skirt:
  +-2 x/z, 8 down — canopy/trunk reach) records covered cells
  GEOMETRICALLY, and the tree gate vetoes candidates whose ground cell
  was carved — no floating trees, and every chunk sharing a tree
  computes the same veto. Carved grass-family columns regrow their own
  surface block on the dirt below.
- Biomes (TerrainGen.cpp): temperature + moisture OpenSimplex fields
  (freq 0.0030, ~300-block features) classify per column: desert
  (t>0.65, m<0.40; deep sand, no trees), snowy (t<0.32; SnowyGrass
  surface), forest (m>0.55; tree chance 0.85/cell), plains (0.18).
  Beach band still overrides near sea level. SnowyGrass is a NEW block
  appended after the flow ids (tiles 10 snow top / 11 snowed side; BOTH
  scripts/gen_textures.py and import_mc_assets.py grew the two layers —
  keep them in sync). Tree chance samples the biome at the tree's own
  position (seam-deterministic). Fixed along the way: trunks now grow
  through neighboring canopies (dense forests exposed that SetIfAir
  left leaf gaps in trunks; gentest caught it).
- Topology (ported from ChunkGeneratorOverworld.generateHeightmap —
  looked up in the local 1.12 source, worth doing again for anything
  similar): per-biome (baseHeight, heightVariation) using vanilla's
  table (desert/plains 0.125/0.05 flat, forest 0.1/0.2, snowy taiga
  0.2/0.2), blended over a 5x5 neighborhood of 4-block biome cells with
  the parabolic kernel 10/sqrt(d^2+0.2), each weight divided by
  (base+2) and HALVED when the neighbor is higher than the center
  (vanilla's sharp-cliff-base trick). A third "ruggedness" field (freq
  0.0035) smoothsteps (0.60..0.85) any cell toward its hills variant
  (extreme hills 1.0/0.5; desert hills 0.45/0.3). height = 16 + base*22
  + n*(5 + variation*34) — flats ~y19+-7, hill peaks toward y59 (world
  ceiling 64; real mountains would need kWorldHeightChunks raised).
  Peaks >= y48 get a SnowyGrass alpine cap (sandy columns excluded).
  Cell params are memoized per Generate call (heightAt blends 25 cells
  per column; caves/trees re-query columns heavily).
- gentest grew: caves carve, carved air never touches water, climate
  produces multiple biomes (spread 5x5 column sample over +-64 chunks),
  snowy grass sits on dirt. Run it after touching TerrainGen/CaveGen.

## M16 flora & decoration (how it works)

Three commits: 37156b7 (stage 1), 084ef24 (stage 2), 0481e5f (stage 3).
Old saves: M16 changed worldgen again (plants, tree species, cactus) —
unedited chunks regenerate with new rules; test in a NEW world.

- Cutout pass (stage 1): NO new mesh stream — cutout blocks live in the
  regular opaque/alpha-tested stream and chunk.frag unconditionally
  discards alpha < 0.5 (solid tiles are 255, water ~0.66, so only real
  holes drop; classic-MC approach). `BlockDef::cutout` cubes (leaves)
  greedy-mesh normally but emit every face against a non-opaque
  neighbor INCLUDING leaf-on-leaf (vanilla "fancy"; the coplanar
  opposite-winding pairs resolve by backface culling). Box-filtered
  mips push leaf alpha toward the tile's ~0.65 coverage, so distant
  canopies fill in solid rather than eroding — no special mip handling.
  Both atlas scripts now keep real leaf alpha (backdrop bake removed).
- Light opacity (stage 1): `BlockDef::lightOpacity` (0..15, ignored
  when opaque) — LightEngine's volume stores per-cell opacity; any
  opacity > 0 ends the direct straight-down sky beam, and BFS spread
  into a cell costs max(1, opacity). Leaves 1 (soft tree shade), water
  3 (vanilla; skylight now fades with depth — the old "sea floor reads
  bright" wart is gone). World.cpp's instant SetBlock light patch is
  untouched (approximate by design).
- Cross plants (stage 2): `BlockDef::cross` + `replaceable`; tall
  grass / dandelion / poppy / dead bush (tiles 12-15). The mesher emits
  per cell (after liquids): an X of two diagonal corner-to-corner
  planes, EACH with both windings (16 verts), AO 3, the cell's own
  light, +Y normal (flat sun response), into the alpha-tested stream.
  Interactions: RaycastBlocks hits solid OR cross (plants targetable,
  never collide); water pours/spreads into replaceable cells (crushes
  plants); falling sand detaches over them and crushes on landing;
  ProcessBlockUpdate pops plants without their soil (earth for
  grass/flowers, sand for dead bush). LOD downsample skips cross blocks
  (sub-cell noise at distance).
- Worldgen plants (stage 2): per-column hash gates (salt 5; species
  pick salt 6) after trees in TerrainGenerator::Generate — chunk-local,
  so seam-deterministic for free; carve-mask veto like trees. Density:
  plains grass 10% / flowers 1.2%, forest 6% / 0.6%, snowy surfaces
  (biome or alpine cap) sparse grass 2% only, beaches bare.
- Tree species (stage 3): TreeSpecies{Oak,Birch,Spruce} — PlaceTree
  takes species; per-species log/leaves ids; trunks grow through ANY
  leaf type. Birch (tiles 16-18, vanilla fixed tint 0x80A755) replaces
  25% of forest oaks (salt 7), trunk 5-7. Spruce (19-21, tint
  0x619961) in snowy biome and on snowline caps, trunk 6-8, conical
  canopy (single tip, alternating radius-1/2 layers) still within
  kCanopyRadius=2 and top+2 — the cell-enumeration and height guards
  rely on that. Oak unchanged (4-6).
- Sandstone (post-stage-3 user request): vanilla's anti-floating-sand
  buffer, ported from Biome.generateBiomeTerrain — sandy columns are 3
  sand over a per-column nextInt(4)-deep sandstone band (hash salt 9),
  then stone; the cave carver's replaceable set includes it. Worldgen
  sand over a carved void still floats until disturbed — that is
  VANILLA-ACCURATE (BlockFalling only reacts to onBlockAdded /
  neighborChanged; generation writes raw chunk data, no updates), the
  band just makes it rare. Tiles 24/25/26 (side/top/bottom).
- Bedrock (post-stage-3 user request, tile 27): vanilla's rule — bedrock
  wherever y <= nextInt(5) (solid y0, ragged through y4; hash salts
  10+wy). `BlockDef::unbreakable` guards the break edit in GameApp (no
  hardness system); NOT in the carver's replaceable set. The user dug
  through the old 1-block floor into the void — that's closed now. (At M16
  the world was only 64 tall with the surface at ~y19-44; M25 later raised
  it to 128 with the surface rebased to ~y68 — see the M25 section, which
  supersedes these height numbers.)
- Cactus (stage 3): full opaque cube (tiles 22/23; import bakes the
  texture's transparent 14/16-inset margin opaque over the body color —
  the real inset model is backlog). Desert sand, 0.5% of columns (salts
  5/8), 1-3 tall, placed in the plant loop; the column is derived
  purely from (wx, wz, height) so vertical-seam chunks regenerate it
  identically (the loop accepts ly down to -2 for that). Block update
  pops segments without sand/cactus below. No adjacent-solid rule, no
  damage (no item/damage systems yet).
- gentest grew: plants generate + sit on proper soil (incl. cactus on
  sand/cactus), and the trunk/leaf checks are generic over all three
  log/leaf species.

## M17 — Survival I: items & inventory (how it works)

CODE COMPLETE 2026-06-12, awaiting user verification. Item economy was
DECIDED with the user: creative palette in the inventory screen +
placing consumes counts + breaking adds the block straight to the
inventory (explicit stopgap that M18's item-entity drops replace).

- Data model (`game/src/Inventory.h/.cpp`): `ItemStack {BlockId, count}`
  (max 64), `Inventory` = 36 slots in vanilla InventoryPlayer order —
  0..8 hotbar (keys 1..9; `Hotbar()` span feeds the HUD), 9..35 the 9x3
  grid. `Add()` merges into matching stacks then first empty slot
  (hotbar first) and returns the leftover. An empty slot IS the empty
  hand — the old "slot 9 = hand" convention survives via the starter
  kit just leaving it empty.
- Persistence: one manifest line, `inventory <n> {slot id count}*n`
  (non-empty slots only) — `WorldSave::Get/SetInventory`, same
  immediate-rewrite pattern as player state. ABSENT line = pre-M17 save
  → EnterWorld grants the legacy starter kit (old hotbar x64);
  `inventory 0` = genuinely empty. savetest covers round-trip + the
  empty-vs-absent distinction. PersistPlayerState merges any carried
  stack back first (window-X while the screen is open is the only quit
  path with one in flight).
- Screen (`game/src/ui/InventoryScreen.h/.cpp`): immediate-mode like
  the menus; GameApp owns inventory/carried/state. Vanilla 176x166
  panel (gui/container/inventory.png imported via COPIES; procedural
  fallback otherwise), slot grid straight from ContainerPlayer: main
  (8+c*18, 84+r*18), hotbar y=142. Armor/crafting regions are inert
  art until M19. Above it a procedural "Blocks" palette panel lists
  every block except air + the 7 internal flow ids: left-click grabs a
  64-stack onto the cursor (replacing it), right-click adds one.
  Slot clicks follow vanilla PICKUP: left = pick/place/swap/merge,
  right = pick larger half / place one / swap on mismatch. Clicking
  outside both panels DISCARDS the carried stack (logged; vanilla
  would throw — needs M18 item entities). Hover = white highlight +
  name tooltip (suppressed while carrying).
- GameApp: `State::Inventory` — cursor free, world KEEPS TICKING
  (vanilla: block updates run, day advances), player physics run with
  input ignored (`Player::Tick(world, dt, input)` gates all key reads
  through `KeyDown()`; you keep falling/floating). E opens (Playing
  only) / closes; Esc closes too (pause is reachable only after
  closing). Closing returns the carried stack via Add (overflow logged
  + discarded) and re-arms the break/place press guards like unpause.
- Economy in HandleInput: place reads the selected hotbar
  ItemStack, decrements (slot empties to {}); break adds
  {brokenId, 1} via Add — no drop tables, blocks yield themselves
  (grass gives grass, tall grass gives tall grass; M18 fixes), full
  inventory loses the block silently.
- HUD: `DrawItemStack` (ui/Widgets) is the shared icon+count drawer
  (side-face tile; count >1 bottom-right at vanilla's x+17/y+17
  anchor); hotbar takes `span<const ItemStack>` now.
- Known M17 limits (fine for now): no hotbar mouse-wheel scroll (engine
  Input has no scroll yet), no number-key slot swap while hovering, no
  shift-click quick-move, water is placeable like any block (bucket is
  far future), bedrock sits in the palette (creative-ish).

What the user should test: E in game → screen layout at their
resolution; drag/merge/split stacks (left/right click); palette grab;
counts on the hotbar; place drains to 0 and the slot empties; breaking
refills; 1..9 selection; E/Esc close; falling keeps happening with the
screen open; quit + re-enter world → inventory restored; an OLD world
still gets the legacy kit and plays as before.

## M18 — Survival II: mining feel (how it works)

CODE COMPLETE 2026-06-12, awaiting user verification. Decided with the
user: fly mode breaks instantly and drops nothing (creative-style);
drop table is vanilla-ish. Replaces M17's two stopgaps (instant pickup
on break, inventory outside-click discard).

- Hardness + drops (Block.h/.cpp): `BlockDef::hardness` (vanilla's
  values: stone 1.5, logs 2.0, dirt/sand 0.5, grass 0.6, leaves 0.2,
  glowstone 0.3, sandstone 0.8, cactus 0.4, plants 0 = instant) and
  `BlockDef::drop` (kDropSelf default, Air = nothing, or an id;
  `ResolveDrop(self)` at use sites). Cross-referencing drops are
  patched after all registrations via `BlockRegistry::EditDef`. NEW
  block: cobblestone (tile 28, stone's drop, never generated). Table:
  stone->cobble, grass/snowy->dirt, leaves/tallgrass/deadbush->nothing,
  flowers/cactus/everything else->self.
- Hold-to-break (GameApp::HandleInput, walk mode): vanilla formula —
  progress += digSpeed/hardness/30 per tick (implemented per-frame:
  frameDt * digSpeed / (hardness*1.5)); digSpeed 1, x1/5 if the eye is
  underwater, x1/5 if airborne (Player::Grounded() accessor). Progress
  resets when the target cell changes or LMB lifts; m_breakCooldown
  doubles as vanilla's 5-tick post-break hit delay. Bedrock/air reset.
  Pause/inventory-open also reset (no frozen cracks).
- Crack overlay: destroy_stage_0..9 live at atlas layers 29..38
  (`blocks::kFirstCrackTile`; BOTH texture scripts grew them + tile 28
  cobble — import thresholds alpha to clean 0/255, replicating
  vanilla's alpha test). Stage = int(progress*10)-1; drawn as an
  inflated (1.004) entity cube over the dig cell with vanilla's
  CRUMBLE blend (`BlendMode::Crumble`, 2*src*dst — the textures are
  dual-shade: dark 61-gray pixels darken the block, light 155-gray
  ones highlight) and u_unlit=1 (the framebuffer is already lit),
  depth writes off; backfaces hide behind the block's opaque mesh.
  First cut drew the texels as opaque lit gray — user caught it.
- block_entity shaders REWRITTEN for shared use: vert is now
  center-based — u_center/u_scale/u_yaw (spin around Y) replace
  u_origin; frag alpha-tests (discard < 0.5) so leaf/plant item minis
  and the crack cutout work. Falling blocks pass center+0.5/scale 1.
- Item entities (World::ItemEntity, World.cpp): vanilla EntityItem
  port — spawn jittered to cell+0.25..0.75 with scatter velocity
  (+-2, 4, +-2 b/s = vanilla's b/tick x20), gravity 16 b/s^2, drag
  x0.98/tick (ground x0.6 horizontal), axis-separated AABB collision
  (0.25 cube), embedded-in-block -> float up (pushOutOfBlocks
  simplified), merges same-id stacks within 0.5 on cell-cross or every
  25 ticks (cap 64), despawns at age 6000. NOT persisted — drops
  vanish on save/quit (they'd despawn anyway). Pickup delay 10 (40 for
  player throws so they don't vacuum straight back).
- Pickup (GameApp::OnTick): World::PickupItems(boxMin, boxMax, take) —
  player AABB grown by vanilla's (1.0, 0.5, 1.0); the take callback
  Add()s to the inventory and returns how many fit, partial leftovers
  stay in the world. Runs in Playing AND Inventory states.
- Render (GameApp::OnRender): one entity pass draws falling blocks,
  item minis (scale 0.25, vanilla bob sin(t*0.1)*0.1+0.1 and spin
  t*0.05 + per-item phase), and the crack cube; cell light sampled
  max(cell, above) as before.
- Crush/pop drops (World.cpp): plants popped by support loss, crushed
  by flowing water (pour-down + sideways sites in UpdateLiquid), or
  squashed by landing sand all SpawnBlockDrop their table entry first
  (CrushDrops checks def.cross).
- Throws: Q tosses one from the hand (press + 0.25 s repeat); the
  inventory screen's outside-click now returns the carried stack via a
  `thrown` out-param and GameApp throws it (eye - 0.3, look * 6 b/s);
  closing with a full bag throws the leftover instead of discarding.
- Known M18 limits: no dig sounds/particles (no audio engine yet), no
  tool speeds until M19, drops aren't saved, falling-sand entities
  crush drops only at the landing cell (vanilla breaks items it falls
  through — close enough), grass digs as fast as dirt bare-handed
  (vanilla parity).

What the user should test (NEW WORLD not needed — worldgen unchanged):
walk mode: hold LMB on stone ~2.3 s -> crack stages -> cobblestone
pops out, scatters, and vacuums in (watch the hotbar count); dirt/sand
~0.75 s; plants instant, flowers drop themselves, tall grass nothing;
leaves break fast and drop nothing; digging while swimming/jumping is
much slower; releasing/retargeting resets the crack. Fly mode: instant
pop, no drops. Q tosses an item forward (pickable after ~2 s). Break a
flower with water flow / falling sand -> it pops as a drop. Inventory
screen: click outside the panels -> stack lands in front of you. Items
despawn after 5 min; quit+reload clears ground items (known limit).

## M19 — Survival III: crafting (how it works)

CODE COMPLETE 2026-06-12, awaiting user verification. Decided with the
user: tool durability IN (vanilla uses: wood 59, stone 131), pickaxe
gating IN (stone family drops nothing bare-handed).

- Item id space (game/src/Item.h/.cpp): `ItemId` (uint16) — ids <
  kFirstItemId (1024) ARE BlockIds; above sit ItemRegistry entries
  (stick + wood/stone pickaxe/axe/shovel, sprite tiles 43..49).
  APPEND-ONLY like blocks (ids persist in level.dat). Helpers span both
  halves: ItemName/ItemIconTile/ItemMaxStack/ItemExists/IsBlockItem.
  `ItemDef{tile, tool, efficiency, maxDamage, maxStack}` — tools stack
  to 1.
- New blocks: planks (tile 39, hardness 2, axe-class; crafted from any
  log) and crafting table (tiles 40 top/41 side/42 front, planks
  bottom, hardness 2.5). Tiles in BOTH texture scripts; GUI sheet
  crafting_table.png joined the import COPIES.
- BlockDef grew `toolClass` (Pickaxe: stone/cobble/sandstone; Shovel:
  dirt/grass/snowy/sand; Axe: logs/planks/table) and `needsPickaxe`
  (the stone trio). Cactus deliberately None (vanilla).
- ItemStack grew `damage`; merges everywhere (Inventory::Add, slot
  clicks, item-entity merge) require id AND damage equal and cap at
  ItemMaxStack. Manifest format: "inventory2 n {slot id count damage}"
  written; M17/M18 "inventory" triples still parse (savetest covers
  both + an item id with damage).
- Recipes (game/src/Crafting.h/.cpp): shaped (anchored anywhere in the
  grid, horizontal mirror, per-cell ingredient ALTERNATIVES — "any
  log") + shapeless (greedy multiset). Starter set (10): any log->4
  planks, 2 planks->4 sticks, 2x2 planks->table, 2x2 sand->sandstone,
  {planks|cobble} x {pickaxe MMM/.S./.S., axe MM/MS/.S + mirror,
  shovel M/S/S}. Recipes::Match(grid span, gridSize) returns the
  result for the UI preview and the craft click.
- InventoryScreen now draws BOTH containers (craft span + craftSize
  param): 2 = player screen (inventory.png, vanilla 2x2 at (98,18),
  result (154,28), palette panel above — palette now lists items too),
  3 = crafting table (crafting_table.png, 3x3 at (30,17), result
  (124,35), no palette). Result click crafts into the cursor (only if
  it fits) and consumes one per non-empty cell. Durability bar in
  DrawItemStack (vanilla 13x2 at (2,13), green->red, shown when
  damaged).
- GameApp: State::Crafting joined Inventory as a "container" state
  (shared OpenContainer/CloseContainer; close returns craft grid +
  cursor to the bag, overflow thrown; window-X persist path merges
  them too). RMB on a crafting table opens it (use beats place, no
  sneak modifier). m_craftGrid is one 9-cell array; the 2x2 uses the
  first 4 cells row-major.
- Tools in the dig path (HandleInput): digSpeed = efficiency (2/4)
  when hand tool class == block toolClass; canHarvest = !needsPickaxe
  || pickaxe in hand — gated blocks use /100 (hardness*5 seconds) and
  drop NOTHING when !canHarvest. Tools wear 1 per broken block with
  hardness > 0 and vanish at maxDamage. Throws/Q preserve damage
  (ItemEntity carries it).
- Non-block item drops render as flat alpha-tested sprite quads
  (m_itemQuad, same entity shader, scale 0.4) instead of mini cubes.
- Known M19 limits: no shift-click quick-move, no furnace/smelting (no
  ores — the natural M20: ore worldgen + iron tier + furnace), no
  recipe book, sticks/tools can sit in the hotbar but place nothing
  (correct), the result slot gives one craft per click (no drag/shift
  mass-craft), no tool-break sound/flash (audio backlog), crafting
  table front texture faces +X regardless of placement (no block
  orientation data yet).

What the user should test (no new world needed): punch a log (slowish
~3 s bare-handed) -> E -> log in the 2x2 -> planks appear in the
result -> craft repeatedly; planks->sticks; 2x2 planks->crafting
table; place it, RMB opens the 3x3; craft a wooden pickaxe (MMM/.S./
.S.) and axe/shovel incl. the mirrored axe; stone WITHOUT a pickaxe:
slow dig (~7.5 s) and NO drop; with the wooden pick ~1.1 s and
cobble drops; stone tools from cobble; durability bar appears after
the first use, tool vanishes at 0 (wood 59 uses); damaged tool
thrown with Q keeps its bar when picked up; quit/reload preserves
damage; sticks render as flat sprites when tossed; old M17/M18 world
loads its inventory intact.

## M20 — Game feel: particles + first-person hand (how it works)

USER-VERIFIED 2026-06-12 ("looks amazing" / "perfect" after the arm
fix below).

- Particles (game/src/Particles.h/.cpp, ParticleDigging port):
  `ParticleSystem` owns a streamed billboard batch (one 2048-quad
  dynamic VBO, CPU-built corners from the camera basis, particle.vert/
  .frag, alpha-tested, depth-tested, NO depth writes, drawn after the
  water pass — vanilla's order). Spawns: destroy burst = 4x4x4 chips
  filling the broken cell, velocity = normalize(offset-center + rand)
  * (rand+rand+1)*0.15*0.4 + 0.1y (b/tick x20); hit chip = one per
  tick while digging, on the dug face 0.1 proud, velocity x0.2 scale
  x0.6. Physics per tick: gravity 0.04 b/t^2, drag x0.98, ground
  friction x0.7, life 4/(rand*0.9+0.1) ticks, axis-separated collision
  vs IsSolid. Texture: random QUARTER of the block's side tile
  (vanilla jitter), tinted x0.6 gray in the shader, lit by
  spawn-sampled cell light. Ticked alongside entities; spawned from
  GameApp's dig/break paths (walk dig chips + burst, fly instant-break
  burst). World-side crushes (water/sand popping plants) do NOT emit
  particles — player edits only, fine for now.
- View model (game/src/ViewModel.h/.cpp + viewmodel.vert/.frag): drawn
  LAST in the world pass over `Renderer::ClearDepth()` (new engine
  call) so it never clips into walls; u_model is a full view-space
  matrix — vanilla's first-person GL chains port verbatim. Held block
  = the shared entity mini-cube (block.json display: rotY 45, scale
  0.4); held item = the shared sprite quad (item/generated display:
  T(1.13,3.2,1.13)/16, rot[0,-90,25], scale 0.68); empty hand = a
  Steve right-arm box mesh (ModelBiped: 4x12x4 px at (-3,-2,-2), rot
  point (-5,2,0), y-down model space — the chain's Rx(200) flips it)
  textured from textures/entity/steve.png (NEW import COPIES entry; no
  skin imported -> empty hand draws nothing, placeholder-safe).
  Animation state ticks at 20 TPS, render-interpolated: swing = 6
  ticks (vanilla), TriggerSwing() queues a restart so held digging
  loops continuously; also fires on fly-break and successful place.
  Equip dip: progress slides +-0.4/tick toward (held == displayed ?
  1 : 0), the displayed item swaps at the bottom of the dip. Lighting:
  eye-cell light + a fixed view-space key light so the cube reads 3D.
- Render order recap (OnRender): sky -> celestial -> opaque terrain ->
  entity cubes/sprites + crack -> water -> PARTICLES -> outline ->
  VIEW MODEL (depth cleared) -> UI.
- Arm orientation fix (user-spotted: "the hand looks backwards"): the
  verbatim vanilla chain showed the teal SHOULDER cap where the fist
  belongs in our matrix setup, so BuildArmMesh applies a rigid 180-deg
  X-rotation about the box center (caps swap, textures ride along).
  Diagnosed by sampling the skin: (44,16) cap is pure teal shirt,
  (48,16) is skin — whichever cap faces the camera tells you the
  orientation.
- Mouse wheel (user request, same session): Window grew a GLFW scroll
  callback + TakeScrollY() accumulator (consumed once per frame in
  OnRender so menu scrolling can't burst-apply); scrolling cycles the
  hotbar, vanilla direction (up = left), wrapping.
- Known M20 limits: no view bobbing, item sprites are flat (vanilla
  extrudes them to ~1px-thick 3D), no swing on Q-throw or attack-miss
  swings (no left-click-in-air swing), arm only for the right hand, no
  particle emission from world-side plant crushes, no scroll in the
  container screens (hotbar only).

What the user should test (visual): bare hand visible bottom-right
(Steve sleeve); hold dirt/stone -> mini block in hand; hold a
tool/stick -> flat sprite held diagonally; 1..9 switching dips the
hand down and back up with the new item; digging swings the arm
continuously AND sprays chips from the face being dug; the break
pops a burst of chips that bounce and settle on the ground; chips
match the block's texture (grass chips greenish from the side tile);
fly-mode instant breaks also burst; particles fade out after a couple
seconds; placing swings once; night: hand and chips dim with the
world. Esc/pause freezes the swing mid-pose; resume continues.

## M21 — Ores + furnace/smelting (how it works)

CODE COMPLETE 2026-06-12, awaiting user verification. Build + gentest
+ savetest all pass; quick title-screen launch was clean (62-layer
atlas, furnace.png loads, clean shutdown). NEW WORLD recommended for
ore hunting (old saves regenerate unedited chunks with veins, but
near-spawn chunks you've edited keep their pre-ore stone).

- Ore worldgen (world/OreGen.h/.cpp, called from TerrainGen right
  after caves::Carve so veins never deposit into carved air): an
  exact WorldGenMinable port — ellipsoid of sin-bulged radius swept
  between two jittered endpoints, every random draw unconditional so
  the sequence never diverges. Seam determinism via the M15 replay
  pattern: each chunk replays the 3x3 origin chunks around it (max
  vein reach ≈ 27 blocks from the origin corner < 32), origins seeded
  with vanilla's population mix (ocx*341873128712 + ocz*132897987541
  ^ seed), ascending visit order so overlapping veins resolve
  identically everywhere. Veins ONLY replace stone. Densities
  rescaled to our 64-tall world (OreParams in OreGen.cpp): coal 10
  veins/chunk size 17 y2..47, iron 12 veins size 9 y2..27 ("dig
  deeper for iron"). The JavaRandom clone moved from CaveGen.cpp to
  world/JavaRandom.h (shared; grew NextDouble) — CaveGen includes it
  now. gentest grew: coal+iron generate, iron stays below y32, ore
  cells never sit in open space.
- New blocks (tiles 50..56, appended in BOTH texture scripts): coal
  ore (drop = coal ITEM — patched in items::RegisterDefaults since
  item ids don't exist at block-registration time, same late-patch
  pattern as stone->cobble), iron ore (drops itself, harvestLevel 1),
  furnace + lit furnace (hardness 3.5, front +X like the crafting
  table; lit emits light 13 and is a separate appended id swapped by
  the burn state — relight rides the normal edit path), glass
  (cutout cube like leaves; glass-on-glass interior faces show where
  vanilla would cull them — accepted). BlockDef::drop is now
  documented as a unified ItemId (uint16 fits both halves).
- Harvest tiers: BlockDef grew `harvestLevel` (0/1/2), ItemDef grew
  `tier` (wood 0, stone 1, iron 2). GameApp's canHarvest is now
  pickaxe-in-hand AND tier >= harvestLevel; everything else (the /100
  dig rate, no drop) unchanged. Iron ore is the only level-1 block.
- New items (tiles 57..61): coal, iron ingot, iron pickaxe/axe/shovel
  (efficiency 6.0, 250 uses — vanilla IRON). Recipes added: iron
  tools (same patterns via the tools() helper), 8-cobble ring ->
  furnace. 14 recipes total. Torches deferred (need a non-cube
  model).
- THE FURNACE (first block entity):
  - `FurnaceState` (world/Furnace.h): input/fuel/output ItemStacks +
    burnTicks/burnTotal/cookTicks. furnace::Tick() is a pure-state
    port of TileEntityFurnace.update (cook 200 ticks; partial cook
    decays -2/tick while unlit; fuel consumed only when a smelt can
    start), returns "burning" and World swaps Furnace/LitFurnace ids
    when that changes. Smelt registry (furnace::SmeltResult): iron
    ore -> ingot, sand -> glass, cobble -> stone. Fuels
    (furnace::BurnTime): coal 1600, planks/logs/crafting table 300,
    wooden tools 200, stick 100.
  - Storage: World-owned `m_furnaces` map (world pos -> state,
    main-thread). Created lazily by World::FurnaceAt (RMB-open does
    it). World::TickFurnaces (from Tick): skips furnaces whose chunk
    is unloaded (state idles in the map), erases stale entries whose
    loaded block isn't a furnace. SetBlock spills all three slots as
    item drops (damage preserved — SpawnBlockDrop grew a damage
    param) and erases the state when a furnace block is replaced by a
    non-furnace id; the lit/unlit swap stays in the family so it
    keeps state. Time does NOT pass for furnaces in unloaded chunks
    or while paused (vanilla-ish enough).
  - Persistence: `furnaces.dat` sidecar in the save dir (text, one
    line per furnace: pos + 3x{id count damage} + burn/burnTotal/
    cook). WorldSave::Get/SetFurnaces; SetFurnaces only marks dirty —
    the debounced Flush writes it (furnaces change every tick while
    burning, unlike the manifest setters). World::SaveEditedChunks
    snapshots the map into the store, so the 30 s autosave and the
    quit path both cover it; empty set deletes the file. savetest
    grew a full round-trip (slots incl. tool damage, progress,
    empty-furnace, file removal).
  - GUI: InventoryScreen::DrawFurnace — vanilla ContainerFurnace
    layout (input 56,17 / fuel 56,53 / output 116,35 take-only) over
    gui/container/furnace.png (added to import COPIES; procedural
    fallback draws the panel + orange burn / white cook bars). Flame
    and arrow overlays are GuiFurnace's exact sub-rects at sheet
    x176, sized by the LIVE state (the world keeps ticking it while
    open). GameApp: State::Furnace joined the container states
    (ContainerOpen covers it: E/Esc close, world ticks, physics run
    input-less); RMB on a furnace stores m_openFurnace and opens.
    OpenContainer(bool) became OpenContainer(State). Closing returns
    only the cursor stack — furnace slots stay in the furnace.
- Torches (same-session follow-up, tile 62, 37 blocks / 15 recipes
  total): floor-standing only — wall mounting waits for block
  orientation data. Coal + stick -> 4 (vertical shaped recipe).
  Block: non-solid, replaceable (water washes it away; CrushDrops
  grew a torch check), emission 14 (the light BFS already seeds
  non-opaque emitters — zero light-engine changes), hardness 0,
  drops itself. Support rule in ProcessBlockUpdate: pops without
  IsSolid below; GameApp refuses placement without solid ground
  (instead of place-then-pop). Raycast targets torches like cross
  plants.
  - THE MESH: vanilla's template_torch — two thin crossed slabs (four
    one-sided full-cell side planes inset 7/16 and 9/16) plus a small
    +Y top cap at the flame height. The texture's middle 2px column
    survives the alpha test, so any angle shows one X plane + one Z
    plane as a 3D post. ORIGINALLY this rode spare bits of the packed
    cubic vertex (data0 28..31 insets + data1 yoff for the cap); M23
    SUPERSEDED that — the torch is now the first client of the float
    model stream (see the M23 section). Its geometry lives in
    `BlockDef::model` (three ModelBox elements) and the cap finally
    sits at the EXACT 10/16 flame height instead of the old
    ninths-quantized ~0.56-biased-low fudge.
  - WHY THE MODEL STREAM (the decision, now executed): vanilla
    represents ALL non-cube blocks (torches, slabs/stairs/fences/panes)
    with arbitrary float positions + sub-tile UVs — its
    DefaultVertexFormats.BLOCK is POSITION_3F + COLOR_4UB + TEX_2F +
    TEX_2S, no packing, no greedy merge. Our 8-byte packed vertex +
    greedy meshing is the right call for cubic terrain (the 99% case)
    and is exactly why sub-cube blocks were awkward. The torch's packed
    inset bits were a legit first accretion, but the line in the sand
    (set with the user 2026-06-12) was: don't grow the packed format
    past the torch — when the second irregular block looms, build a
    second float vertex stream and subsume the torch into it. M23 did
    exactly that.
  - Sprite rendering (bonus fix): new `RenderAsSprite(ItemId)`
    (Item.h) — registry items PLUS cross/torch blocks now draw as
    flat quads instead of mini cubes, both as world drops (GameApp
    entity loop) and in the first-person hand (ViewModel,
    item/generated transform). Held flowers stopped being cubes too.
- Known M21 limits: furnace front faces +X regardless of placement
  (same as the crafting table), no charcoal/no XP, lit furnace sits
  in the creative palette (placing it gives an always-lit furnace
  until something opens/ticks it to unlit), no shift-click
  quick-move into slots, no hopper-style automation (far future),
  torches are floor-only and don't flicker (no particle flame —
  pairs with the audio/ambience milestone).

What the user should test (NEW WORLD recommended): dig a cave or
strip-mine — coal ore at any depth, iron in the lower half (y<63 after
M25's rebase; was y<28, i.e. well
below the surface); coal ore with a wooden pick pops the coal ITEM
(flat sprite); iron ore with a WOODEN pick: slow dig, NO drop; with a
stone pick it drops the ore block itself. Craft a furnace (8 cobble
ring in the 3x3), place it, RMB opens the GUI: ore in the top slot,
coal (or planks/sticks) below -> flame lights, front texture glows,
arrow sweeps ~10 s per item, ingots land in the output (take-only
slot). Furnace keeps smelting with the GUI closed (watch the lit
front + light it casts at night) and goes dark when fuel/input runs
out. Smelt sand -> glass (place it — see-through cutout cube) and
cobble -> stone. Iron tools from 2 sticks + 3 ingots: noticeably
faster (6x) digging, durability bar wears over 250 uses. Break a
loaded furnace mid-smelt -> contents spill as drops + the furnace
block drops. Quit + reload mid-smelt -> slots, burn, and progress
survive. An old M19/M20 world still loads fine (its furnaces.dat is
just absent).

Torches: craft (coal over stick -> 4), place on the ground — reads
as a thin 3D post from every angle, with a small lit flame cap on top
when you look down at it (added after the first cut missed it; first
placement floated above the post, fixed by dropping kCapYOff to sit
just under the flame top — the height is ninths-quantized so it can't
land pixel-exact on the texture's flame, and biasing below avoids a
gap). Lights the area (14, just below glowstone) and looks right
in caves at night; held torch is a flat sprite in hand (so are flowers
now). Can't place on the side of a wall or on another torch (no
ground); dig out the block under one -> it pops as a drop; flowing
water washes it away (drop pops). Breaking is instant; quit/reload
keeps placed torches (they're normal blocks).

## M22 — Audio engine (how it works)

USER-VERIFIED 2026-06-13 ("beautiful, it all works"). Build + gentest +
savetest all pass; a clean-exit launch was good (device opened at 48 kHz,
113 SFX clips decoded in ~220 ms, 12 music tracks kept as paths, 82 ticks
of gameplay, clean "audio shutting down"). Scope DECIDED with the user:
"Full pass" — SFX + footsteps + ambient + furnace crackle + music.

- ASSET FINDING (the prior session was RIGHT — I briefly doubted it and
  was wrong): vanilla `.ogg`s ARE in the MCP source, in the launcher-style
  HASHED object store at `D:\Minecraft source code\mcp940\jars\assets`.
  `indexes/1.12.json` maps a name ("minecraft/sounds/dig/stone1.ogg") to a
  SHA1 hash; the bytes live at `objects/<hash[:2]>/<hash>` with NO `.ogg`
  extension — which is why a naive `find -iname *.ogg` (or a `sounds.json`
  search) turns up nothing. The 1.12 index has 1085 sound entries:
  `sounds/{dig,step}/<mat>N.ogg` (cloth/grass/gravel/sand/snow/stone/wood),
  `sounds/random/{pop,glass1-3}`, `sounds/ambient/cave/cave1-18`,
  `sounds/liquid/{splash,splash2,swim,...}`, `sounds/fire/{fire,ignite}`,
  `sounds/music/game/{calm,hal,piano,nuance}*`. (The user's Technic install
  has a 1.7.10 `virtual/legacy` store with real filenames too — equivalent
  for these sounds — but we use mcp940 since it's the same canonical source
  tree as the textures and needs no launcher install.)
- ENGINE (`engine/src/vox/audio/AudioEngine.{h,cpp}`, namespace `vox`):
  PIMPL facade over miniaudio's high-level engine (mirrors how
  `vox::Renderer` hides glad — miniaudio.h never leaks into the header;
  linked PRIVATE). Backend chosen after checking: miniaudio's built-in
  decoders are WAV/FLAC/MP3 only, and its example Vorbis backends need
  external libvorbis. So we PRE-DECODE each `.ogg` to s16 PCM with
  stb_vorbis (`stb_vorbis_decode_filename`) at load and feed miniaudio
  via per-voice `ma_audio_buffer_ref` (each ref carries its own read
  cursor, so one cached clip plays concurrently). No custom decoding
  vtable, no resource-manager config, no external deps. API: `Init`/
  `Shutdown` (idempotent; Init returns false -> silent no-op engine if
  no device), `LoadClip` (decode+cache; missing file -> invalid handle,
  silent), `Play2D`/`Play3D` fire-and-forget one-shots (volume+pitch),
  `PlayLoop3D`/`PlayLoop2D` + `StopVoice`/`FadeOutVoice`/`SetVoice*`/
  `IsVoiceActive` (generation-tagged `VoiceHandle` -> stale Stop is a
  safe no-op), `SetListener` (per frame from the camera),
  `SetBusVolume` (Master via engine volume, Sfx/Music/Ambient via
  `ma_sound_group`s), `Update` (reaps finished one-shot voices + advances
  fades — voices are ONLY uninited here, never from a trigger site).
  3D voices use linear attenuation, min 4 / max 48 blocks.
- THIRD PARTY (`third_party/miniaudio/`): vendored `miniaudio.h`
  (0.11.25) + `stb_vorbis.c` (public domain) + `miniaudio_impl.cpp`
  (the single TU defining `MA_IMPLEMENTATION` + `MA_NO_ENCODING` — the
  ODR guard; stb_vorbis.c compiles as its own C TU). CMake: a `miniaudio`
  static lib (+ `ole32 winmm` on WIN32). These ARE committed (like glad);
  the `.ogg`s are gitignored.
- GAME taxonomy: `BlockDef::soundType` (`SoundType` enum: None/Stone/
  Wood/Grass/Gravel/Sand/Snow/Cloth/Glass — vanilla StepSound classes),
  assigned in `blocks::RegisterDefaults` and the Plant/Log/Leaves
  helpers (dirt->Gravel like vanilla, cactus->Cloth, water/flows->None,
  glass->Glass). `vc::GameSounds` (`game/src/audio/Sounds.{h,cpp}`) owns
  every `ClipHandle`, loads from `vox::assets::Resolve("mc/sounds/...")`
  (the game owns the overlay-prefix policy; engine just loads a path),
  probes up to 6 variants per material (keeps the ones that exist), and
  exposes semantic triggers that pick a random variant + jitter pitch:
  `PlayDig` (mining tick, quiet/low), `PlayBreak` (loud; Glass ->
  random/glass shatter), `PlayPlace`, `PlayPickup` (random/pop, high
  pitch), `PlayStep`/`PlayLand`, `PlaySplash`, `StartFurnaceLoop`/
  `StopFurnaceLoop`, `UpdateAmbient`, `UpdateMusic`.
- WIRING (`GameApp`): `m_audio` + `m_sounds` members (m_audio declared
  first so it outlives m_sounds; `OnShutdown` also calls
  `m_audio.Shutdown()` explicitly before teardown). `OnInit`:
  `m_audio.Init(); m_sounds.Load(m_audio)`. `OnRender` (in the
  `if (m_world)` block): listener follows the camera + `m_audio.Update`;
  then while Playing/container-open: a FURNACE-LOOP RECONCILE
  (`World::ForEachLitFurnace` — a template iterating m_furnaces whose
  block == LitFurnace, skipping unloaded chunks; GameApp diffs against
  `m_furnaceLoops` keyed by ivec3, starts/stops voices — World stays
  audio-free), cave ambient when the eye-cell packed light is dark
  (sky&block both < 4), and sparse music (`AudioEngine::PlayMusic` decodes
  one track on demand on the Music bus, ~6-12 min start-to-start gap,
  gated on `!MusicActive()`). `HandleInput`: dig sound on the existing
  ~4-tick mining cadence (`m_digSoundAccum`, re-armed when the dig cell
  changes), break sound at destroy (survival + fly branches), place
  sound after the place `SetBlock`. `OnTick`: pickup pop in the
  PickupItems accept path (<=1/tick), and footsteps/landing/splash from
  `Player` state — added `Player::InWater()` (caches the existing
  `inWater` from TickWalk; false in fly) feeding stride-based footsteps
  (~1.7 blocks while grounded & dry, block-under-feet picks the set),
  a landing thud on the grounded transition, and a splash on entering
  water. `EnterWorld` re-seeds footstep tracking; `ExitToTitle` stops
  all furnace loops before the world drops.
- KNOWN M22 LIMITS / future: SFX clip load is synchronous at startup
  (~220 ms for 113 clips — the miniaudio lib, incl. stb_vorbis, is forced
  to /O2 even in Debug via third_party + the C-flags /RTC1 strip in the
  root CMakeLists, else ogg decode was ~8.8 s; could async-load if it
  grows); music decodes one track on demand (a brief hitch when a track
  starts, every ~10 min — acceptable; could thread it) and has no
  crossfade (long gaps mean tracks never overlap); cave ambient uses a
  simple darkness gate + random timer, not vanilla's exact probability;
  no UI to set bus volumes yet (the buses + `SetBusVolume` exist — a
  settings screen is the natural home); footsteps don't vary by walk vs
  sprint cadence beyond the distance threshold; no block-place "use"
  sounds for doors/etc (no such blocks yet); ladder/anvil/metal sound
  classes unused (no such blocks). Debug builds log nothing for missing
  clips (expected: variant probing + clean clone) — the GameSounds
  "loaded sound sets (music tracks: N)" line is the success signal.

What the user should test (NO new world needed; assets already imported
via `python scripts/import_mc_assets.py`): dig any block -> a per-material
dig sound repeats while mining, a louder break sound at the moment it
pops; place a block -> place thunk; walk and sprint over grass/stone/sand/
gravel -> footsteps match the surface; jump and land -> a thud; walk into
water -> a splash; pick up a dropped item -> a high "pop"; craft + place a
furnace, light it -> a fire crackle loop you can hear positionally (louder
up close), that stops when the fuel/input runs out, when you break it, or
when its chunk unloads (walk far away); stand in a dark cave -> occasional
cave ambience; leave the game running a while -> a music track every several
minutes. Fly mode break still pops + sounds. Quit to title -> furnace loops
stop. A machine with no audio device (or a clean clone with no
`assets/mc/sounds/`) should run totally silent without crashing.

## M23 — model-block stream (how it works)

USER-VERIFIED 2026-06-13 ("everything looks perfect and the torch cap is
where it should be"). This is the "right way" the M21 torch notes promised:
a SECOND chunk vertex stream for non-cube geometry, so fractional blocks
stop stealing bits from the packed cubic vertex. Torch is its first client;
slabs/stairs/fences/panes are the foreseen next ones.

- THE STREAM: `ChunkMesh` grew `modelVertices` alongside `vertices`
  (opaque/cutout/cross) and `transparentVertices` (liquids). `ModelVertex`
  (ChunkMesher.h) is 24 B — float x/y/z (chunk-local block units, cell +
  fraction), float u/v (tile-space UV, 1.0 = one tile, sub-rects sample
  within a tile), and one packed uint (layer:16 | normal:3 | ao:4 | sky:4 |
  block:4). Decoded by `assets/shaders/model_block.vert`, which emits the
  SAME varyings as chunk.vert (v_normal/v_uvw/v_worldPos/v_ao/v_sky/v_block)
  so both streams share `chunk.frag` unchanged (alpha-tested, lit, fogged).
- THE POOL: a second `vox::MeshPool` (`World::m_modelPool`, accessor
  `ModelMeshes()`) because its layout differs from the cubic one
  (Float3 + Float2 + UInt vs two UInts). The engine MeshPool is fully
  generic — zero engine changes; `ModelVertexLayout()` + a smaller initial
  capacity (models are rare) is all it took. `ChunkEntry` carries a third
  handle `meshM`/`indexCountM`; `UploadModelMesh` mirrors `UploadMesh`;
  `processMesh` uploads it (counted in the same per-frame upload budget);
  unload frees it. LOD has NO model stream (torches/cross are skipped at
  LOD distance, same as before).
- THE DRAW: `CollectVisibleChunks` gained an `outModel` list, filled at the
  same sites as opaque (same per-draw vec4: chunk min + scale 1). GameApp
  draws it right AFTER the opaque cubic pass and BEFORE the entity cubes /
  water, with `m_modelShader` (model_block.vert + chunk.frag) and the same
  lighting/fog uniforms — opaque GL state (cull on, depth write on, blend
  off), since model faces are alpha-tested cutouts.
- BLOCK MODELS: `BlockDef::model` is a `std::vector<ModelBox>` (Block.h).
  A `ModelBox` is a vanilla-style "element": `from`/`to` in 1/16 "pixel"
  units (0..16) + a per-face {on, tile, uv-rect (0..16 px)} in BlockFace
  order. Empty = ordinary cube/cutout/cross/liquid. The mesher's per-cell
  pass (after liquids/cross) calls `EmitModelCell` -> `EmitModelBox` per
  box per enabled face: reuses the cube mesher's FaceBasis for CCW-outward
  winding and v-up texture orientation, so full-tile faces render exactly
  like cube faces (the torch sides exercise only full-tile UVs; arbitrary
  sub-rect UV orientation is supported by the format but should be eyeballed
  when slabs first use it). Lighting is the cell's own light (max with the
  block's emission) and full-bright AO — these are small decorative shapes,
  not surfaces wanting neighbor gradients.
- THE TORCH (ported + bug fixed): `blocks::Torch` now defines three
  ModelBoxes — two thin crossed full-height slabs (the four outward side
  planes, tile 62) and a top cap. The cap is a +Y face of box
  [7,0,7]->[9,10,9], so it sits at the EXACT 10/16 flame height (vanilla's
  central-post upper face), sampling the flame-core sprite (tile 63) — this
  fixes the M21 wart where the cap rode the ninths-quantized yoff field
  (~0.56) and had to be biased low to hide a gap. `BlockDef::torch` survives
  as the GAMEPLAY flag (targetable, never collides, needs solid ground,
  washes away); the SHAPE moved entirely to `model`. The old `EmitTorchCell`
  and the data0 xIn/zIn inset bits + chunk.vert `kInset` table are GONE;
  data0 bits 28..31 are free again. gentest's torch check now asserts the
  model stream (no cubic geometry, 4 planes + 4-vert cap, cap at 10/16).
- WHEN ADDING A SLAB/STAIR (the playbook): give the block a `model` (its
  boxes), leave `opaque=false`, add a per-cell flag path only if a new
  gameplay rule is needed; the mesher + pool + draw already handle it.
  Watch: real per-face light sampling (current code uses own-cell light —
  fine for torches, probably too flat for a big slab face), partial
  COLLISION boxes in Player physics (torches dodge this by never colliding;
  slabs/stairs can't), and LOD (still skipped — fine for small blocks).
  Sub-rect UV orientation: verify visually the first time a half-tile face
  is used.

## M24 — block orientation / facing (how it works)

CODE COMPLETE 2026-06-13, awaiting user verification. Build + gentest +
savetest all pass. This is the general per-block-metadata mechanism the
handoff promised since M19 ("no block orientation data" wart), with furnace
+ crafting table fronts and wall torches as the first three clients. The
per-cell meta layer it adds is what SLABS/STAIRS/rotated-logs will reuse.

- STORAGE (`Chunk.h`): a parallel `std::array<uint8_t, kVolume> m_meta`
  alongside the ids, with `Get/SetMeta` + `RawMeta()`. Chunk grew 8 KB ->
  12 KB. Meta is block-SPECIFIC (vanilla's meta-int idea): 0 everywhere =
  unoriented, which is exactly what worldgen and pre-M24 saves produce, so
  defaults are unchanged. Copy-on-write carries it for free — the default
  Chunk copy ctor copies both arrays, and `SnapshotFor` already shares
  `shared_ptr<const Chunk>`, so meta rides into worker snapshots with no
  new locking.
- META SEMANTICS + helpers (`Block.h` `namespace facing`):
  `Dir(BlockFace)`, `Opposite(BlockFace)`, `HorizontalFromLook(vec3)`
  (vanilla EnumFacing.fromAngle, snapped to the dominant cardinal axis),
  and torch packers (`TorchFloor=0`, `TorchWallMeta(face)=face+1`,
  `TorchIsWall`, `TorchWallFacing`). Two block-specific encodings:
  - horizontalFacing cubes (furnace/table): meta = the BlockFace of the
    FRONT (PosX=0 default = the old +X front, NegX=1, PosZ=4, NegZ=5).
  - torch: meta 0 = floor (standing), else `face+1` where `face` is the
    horizontal direction the torch POINTS (away from the wall it hangs on).
- EDIT PATH (`World::SetBlock`): a `SetBlock(pos, id, meta)` overload (the
  old 2-arg form delegates with meta 0, so every existing caller is
  unchanged); the early-out now also compares meta, and the COW clone
  `SetMeta`s the cell. `World::GetMeta(wx,wy,wz)` mirrors `GetBlock`.
- PERSISTENCE (`WorldSave`): two new chunk-blob formats — 2 = format-0 id
  RLE then a meta RLE stream, 3 = format-1 raw ids then the meta RLE
  (`{metaByte, run}` pairs, kept u16-aligned like the id stream). They're
  written ONLY when some cell has non-zero meta, so an unoriented chunk
  stays bit-identical to a pre-M24 blob (format 0/1) and old saves decode
  meta = 0. `Decode` was refactored around a shared `decodeRle` lambda.
  savetest asserts: unoriented chunk keeps the legacy format, oriented
  chunk uses 2/3, legacy blob decodes to all-zero meta, oriented chunk
  round-trips ids AND meta.
- MESHER (`ChunkMesher.cpp`): `PaddedVolume` gained a `meta` array
  (FillPadded copies it). Two consumers:
  - CUBE front remap: `OrientedFaceTile(def, meta, face)` — for a
    `BlockDef::horizontalFacing` cube it draws the canonical front tile
    (`faceTiles[PosX]`) on whatever horizontal face the meta names and the
    side tile (`faceTiles[NegX]`) on the other three; top/bottom unchanged.
    Used in the greedy mask key, so differently-facing neighbors get
    different keys and never wrongly merge.
  - MODEL orient: `EmitModelBox` now takes a `glm::mat4` cell-local
    transform; `ModelOrientation(def, meta)` returns identity for floor
    torches / unoriented model blocks, and for a wall torch a rigid
    tilt+shift (tilt 22.5 deg toward the facing about a low pivot, base
    shoved -0.45 toward the wall + 0.10 up — EYEBALLED constants, tweak if
    it sits wrong). Rigid motion preserves winding/backface culling; the
    face-index normal is kept (slightly off sun term on tilted faces,
    invisible on a torch).
- PLACEMENT (`GameApp` place path): torch on a TOP face -> floor (solid
  below required); torch on a SIDE face -> wall torch pointing along the
  clicked normal if that wall block is solid; ceiling face refused.
  horizontalFacing cube -> meta = `Opposite(HorizontalFromLook(forward))`
  (front toward the placer). The chosen meta goes to `SetBlock`.
- BLOCK UPDATES (`World::ProcessBlockUpdate` torch rule): now facing-aware
  — a floor torch (meta 0) still needs solid ground below; a wall torch
  checks the block in `-facing` (the wall it hangs on) and pops as a drop
  when that goes away.
- `BlockDef::horizontalFacing` flag set on furnace, lit furnace, crafting
  table (their `faceTiles[PosX]`/`[NegX]` were already the front/side
  tiles, so nothing else changed there).

KNOWN M24 LIMITS / decisions: wall-torch tilt/shift constants are
eyeballed (not pulled from the hashed-store JSON, which isn't extractable
by filename) — visually tunable in `ModelOrientation`. No collision box for
oriented blocks (furnace/table are full cubes; torches never collide).
Tilted model faces keep their axis-aligned normal index (fine for a torch).
LOD has no model/oriented blocks (worldgen produces none). Slabs/stairs
(half/shape state + partial collision) are deliberately OUT of scope — they
reuse this meta layer but are their own later milestone.

WHAT THE USER SHOULD TEST (no new world needed — worldgen unchanged, old
saves render as before): place a furnace / crafting table from several
angles -> the front always points back at you; break + replace to
re-orient. Click the SIDE of a solid block with a torch in hand -> it
mounts on the wall, tilted out, and lights the area; click a TOP face ->
floor torch; aim at a ceiling -> placement refused. Dig out the wall a
wall-torch hangs on -> the torch pops as a drop; dig the floor under a
standing torch -> still pops. An old save's furnaces/tables keep their +X
front and old floor torches are unaffected.

## M25 — deeper world (how it works)

CODE COMPLETE 2026-06-13, awaiting user verification. Build + gentest +
savetest all pass. Decided with the user: 128-tall (8 chunks), NOT full
vanilla 256 — 128 gives vanilla's actual UNDERGROUND depth (~63 blocks of
stone/caves/ore from surface to bedrock) without quadrupling streaming cost
on empty air. NEW WORLD REQUIRED: worldgen heights/sea level changed, so
unedited chunks in an old save regenerate with the new rules and seam
against any previously edited chunks (same caveat as M15/M16). This
supersedes the height/sea-level/ore-band numbers quoted in the M11/M15/M16
and M21 sections above.

- HEIGHT (`Light.h`): `kWorldHeightChunks` 4 -> 8 (`kWorldHeightBlocks`
  64 -> 128). Everything structural derives from this constant — LOD,
  lighting, snapshots, occlusion culling, save format, draw lists all just
  scaled. The only knock-on constant: `World.h`'s `kLodHeightChunks` is now
  `kWorldHeightChunks / 2` (= 4; a static_assert guards it), and the
  LodColumnEntry mesh-handle arrays are filled via a new `InvalidMeshes<N>()`
  helper instead of a hardcoded 2-element brace list (kInvalidMesh is
  0xFFFFFFFF, not 0 — value-init would alias real slot 0).
- SURFACE REBASE (`TerrainGen`): `kSeaLevel` 14 -> 63 (vanilla). The
  heightmap formula became `kSeaLevel + 3 + base*18 + n*(3 + variation*30)`
  — the +3 lifts flat plains (~y68) clearly above the beach band (sea+2 =
  65) so inland flats stay grass instead of turning to sand (the first cut
  without the lift dropped tree coverage ~5x — gentest's log count caught
  it). Flats ~y66-73, forests roll more, extreme hills top out ~y102.
  `kSnowLine` 48 -> 90 (only tall peaks cap with snow now).
- SPAWN (`GameApp` `kSpawnPos`): y 48 -> 104, above the tallest terrain
  (~y102), so a fresh world drops the player onto the surface rather than
  inside a hill. No fall-damage system, so the short drop is harmless. (A
  ground-search spawn would avoid the drop but needs the spawn chunk loaded,
  which it isn't yet at EnterWorld — deferred.)
- ORES (`OreGen`): rebased to essentially vanilla now that the surface sits
  near vanilla's y64 — coal 20 veins size 17 y2..124 (whole underground),
  iron 20 veins size 9 y2..62 (lower half; was the deep-only y2..27). Vein
  horizontal reach is unchanged (size 17 -> ~27 < 32), so the 3x3 origin
  replay still resolves seams identically. gentest's iron-band assertion
  loosened from `< 32` to `< kWorldHeightBlocks/2 + 8`.
- CAVES (`CaveGen`): tunnel start height `nextInt(nextInt(54)+8)` ->
  `nextInt(nextInt(108)+8)` (doubled with the world, same low bias) so most
  tunnels sit deep and a few reach up toward the surface. The analytic
  ocean-breach test and the y clamp are sea-level/height-derived, so they
  followed automatically.
- KNOWN M25 LIMITS / decisions: no empty-air-chunk fast path, so chunks
  above the surface still generate/light/mesh (cheap — interior stone meshes
  to nothing, cave-culling skips sealed chunks, air meshes empty — but the
  gen/light jobs still run); doubling height roughly doubled RAM and the
  debug-build streaming-burst fps dip (release is fine; user accepts debug
  dips per the standing note). No lava on deep cave floors yet (no lava
  block — backlog). Hills cap ~y102 (kept below spawn); true vanilla
  extreme-hills reach higher but mountain height wasn't the goal (digging
  depth was). 256-tall remains an option later if empty-section culling
  lands first.

What the user should test (NEW WORLD required): make a New World -> you
fall a short way onto the surface (~y68). Dig straight down -> a long
column of stone with caves and ore (coal throughout, iron in the lower
half) all the way to bedrock at y0 — far deeper than before. Surface looks
normal (grass plains a few blocks above the water, oceans/beaches at the
shoreline, snow only on tall peaks). Caves feel deep and extensive. An OLD
save still loads but its untouched chunks regenerate at the new heights and
will seam against previously-edited areas (expected — start fresh).

## M26 — lava (how it works)

CODE COMPLETE 2026-06-13, awaiting user verification. Lava as a real fluid:
emissive, slow-spreading, pooling on deep cave floors, with the lava/water
interaction blocks. Decided with the user (scope): (a) FULL vanilla mixing
incl. a new OBSIDIAN block harvested with the iron pickaxe (no diamond tier
yet); (b) NO lava damage (no health system — you can swim in it harmlessly);
(c) surface lava lakes DEFERRED (deep cave-floor lava is the headline).
Reuses the M13 liquid engine and M11 partial-height liquid render; the work
was generalizing the water-only `World::UpdateLiquid` to a per-liquid param
set and the cave-floor worldgen.

- BLOCKS (`Block.cpp`/`Block.h`): `Lava` source + `LavaFlows[7]` flow ids
  mirror `Water`/`WaterFlows` (APPEND-ONLY — ids persist in saves), all at
  tile 64 (lava still, used for source AND every flow level like water),
  `liquid`, `opaque=false`, `solid=false`, `emission=15`, `lightOpacity=15`
  (light can't pass THROUGH lava — vanilla full blocker — but emission still
  floods out, lighting cave floors), `soundType=None`. `Obsidian` (tile 65):
  hardness 50, Pickaxe class, `needsPickaxe`, `harvestLevel=2` (iron+). NEW
  BlockDef field `liquidSource` tags each liquid id with its family's source
  id (0 on non-liquids) — the flow engine uses it to tell water from lava.
- FLOW ENGINE (`World::UpdateLiquid`, generalized): a `LiquidParams` lookup
  ({source, flows, decay, slopeFind, infinite, lava}) keyed off
  `liquidSource`. Water = {decay 1, slopeFind 4, infinite ON}; lava =
  {decay 2, slopeFind 2, infinite OFF}. The whole rule set is now
  family-aware — only SAME-family neighbors feed a cell's level / count as
  spread targets; a different-family neighbor triggers mixing instead.
  `SlopeDistance` takes the source id + max distance. `wake()` (in SetBlock)
  schedules free-flowing lava at 30 ticks (vanilla overworld) vs water's 4 so
  it oozes — BUT a lava cell that touches water is scheduled FAST (2 ticks),
  because vanilla's mixing (`checkForMixing`) fires the instant a neighbor
  changes, not on the slow lava tick. Spread strength: source pushes
  `8 - decay` (water 7, lava 6 → lava reaches ~3 cells), falling columns 7,
  other flows `level - decay`.
- MIXING (`World::CheckLavaMixing`, from `BlockLiquid.checkForMixing` +
  `BlockDynamicLiquid`): a lava cell touching water on any of its 5 non-down
  faces solidifies FIRST in its update — a SOURCE → obsidian, ANY flow →
  cobblestone. Lava pouring straight DOWN onto water turns the WATER cell to
  STONE (vanilla's special down case). Water never flows into lava (a
  different family is not a spread target), so the boundary always resolves
  via the lava side. Place water beside lava → wakes the lava (fast) →
  obsidian/cobble. VANILLA-ACCURATE threshold: SOURCE → obsidian, strong flow
  (our level ≥ 4) → cobblestone, weakest tip (our level < 4) → nothing (two
  pools that spread together only along their thin edges make no rock — same
  as real MC, where cobblestone generators rely on the flow continuing). The
  user briefly tried "any flow cobbles" but reverted to vanilla. NOTE: a
  boundary that already SETTLED (no pending updates) won't retroactively
  convert — only freshly-placed/flowing liquid triggers it.
- RENDERING: lava uses `EmitLiquidCell` (BlockDef::liquid) for the
  partial-height sloped surface, but `BlockDef::liquidOpaque` routes its
  faces into the OPAQUE vertex stream (depth-write on, back-face cull on)
  instead of the blended transparent pass. First cut left it in the blended
  pass like water and it rendered inside-out (no depth write + no cull on a
  fully-opaque block — user caught it); the opaque pass sorts it correctly
  and back-face culls the inner faces. Same packed ChunkVertex format + same
  chunk.vert/frag (the surface-drop yoff and the alpha test both still
  apply), so the only difference is draw state. Texture alpha is 255 (the
  importer forces `lava_still` opaque). Full-bright comes from the light
  engine seeding block-light 15 in the lava cell (emission). Water stays
  translucent in the blended pass (`liquidOpaque` false).
- WORLDGEN (`CaveGen.cpp` carve): deep carved cells fill with a lava SOURCE
  instead of air when `wy <= 10` (vanilla `MapGenCaves.digBlock`'s
  `j2-1 < 10` rule) — pools sit statically on the cave floor (raw chunk
  data, no scheduled updates, like worldgen oceans) until disturbed. The
  analytic ocean-breach test is unaffected (it's about water, well above
  y10). gentest gained "lava pools generate below y10" + "no worldgen lava
  above y10" (both pass).
- PLAYER (`Player.cpp`): a body-cell `liquidSource == Lava` check makes lava
  swimming molasses — horizontal drag 0.30 (water 0.55) and the vertical
  swim/sink kicks scaled by that ratio. No damage (decision b).
- UI/FOG (`GameApp.cpp`): `EyeInWater()` is now water-FAMILY only; new
  `EyeInLava()` drives a thick orange screen tint (0.78,0.24,0.04 @ .72) and
  dense glowing-orange fog (range 0.25..2.5) on both the chunk and model
  shaders. `EyeLiquid()` is the shared eye-block lookup.
- PALETTE: lava SOURCE shows (the palette already lists only liquidLevel 0
  or 8); the 7 lava flow ids are skipped for free. Obsidian shows (solid).
- BUCKETS (M26 follow-up, added on user request for liquid pickup/dump):
  three new ItemRegistry items (`items::Bucket` / `WaterBucket` / `LavaBucket`,
  sprite tiles 66/67/68 in both atlas scripts; empty stacks to 16, filled to
  1). Recipe: three iron ingots in a V (`I I` / ` I `). Use logic is
  `GameApp::TryUseBucket()` (called from the RMB place path, AFTER the
  crafting-table/furnace open checks so block-use still wins, BEFORE normal
  placement, and OUTSIDE the `m_target` guard so it works aiming straight at
  liquid). Empty bucket: a dedicated `RaycastBlocks(..., includeLiquids=true)`
  (the crosshair ray still skips liquids) finds the aimed cell; if it's a
  liquid SOURCE (`FilledBucketFor` != 0) it's removed and the matching filled
  bucket is produced (peeled off a stack via `Inventory::Add`, tossed if no
  room). Filled bucket: places its source (`BucketLiquid`) at `m_target +
  normal` if empty/non-colliding and becomes an empty bucket. Flowing liquid
  can't be scooped (only level-8 sources). `items::BucketLiquid` /
  `FilledBucketFor` are the data-driven id↔block maps. Buckets appear in the
  creative palette automatically (registry items) and round-trip in saves
  (item ids). New-world starter kit seeds slots 9-11 with an empty bucket +
  pre-filled water/lava buckets for testing. SOUNDS: vanilla
  item/bucket/{fill,empty}[_lava]{1..3} — the importer's want_sound families
  grew `item/bucket/`, GameSounds loads four ClipSets and exposes
  PlayBucketFill/Empty(bool lava, pos), wired into TryUseBucket (lava picks
  the _lava variant). Re-run import_mc_assets.py for the .ogg files.

SCOPE BOUNDARY: NOT in scope — lava damage (no health system), Nether-style
fast lava, surface lava lakes (deferred), fire/flammability. (Buckets were
originally out of scope but were added as the follow-up above.) OLD SAVES:
worldgen (cave lava) + new append-only block/item ids — a NEW WORLD shows the
cave lava; old saves keep their already-generated (lava-free) caves, new
chunks get it.

WHAT THE USER SHOULD TEST (NEW WORLD): dig to the bottom → glowing lava
pools on cave floors below ~y10, lighting the area, bright from every angle.
From the creative palette: place a lava source → it spreads ~3 blocks
(shorter than water) and SLOWLY. Pour water onto flowing lava → cobblestone;
water onto a lava source → obsidian; flowing lava down onto water → stone.
Mine obsidian: only an iron pickaxe harvests it (slowish). Swim in lava →
sluggish (and harmless for now); eye in lava → orange blinding tint + fog.
Quit/reload → placed lava + obsidian persist. (Re-run import_mc_assets.py
first so the real lava/obsidian/bucket textures are in the overlay.)
BUCKETS: a new world starts with buckets in the inventory grid (press E).
Craft one from 3 iron ingots (V shape). Empty bucket: aim at a water/lava
SOURCE + RMB → it fills (aiming at a FLOW does nothing — sources only).
Filled bucket: RMB on a block face → dumps the source there + empties. Note
the lava-source obsidian rule means a water bucket emptied onto a lava SOURCE
makes obsidian.

## M27 — water generation: lakes + oceans (CODE COMPLETE)

CODE COMPLETE 2026-06-14, awaiting user verification (build + gentest +
savetest pass). PART A (oceans) was verified by the user ("looks good"); PART B
(lakes) is code-complete awaiting verification. See the PART A / PART B
sections below for exactly what shipped. NEW WORLD REQUIRED (worldgen changed).

DECIDED with the user 2026-06-14 as the next milestone. The user noticed the
post-M25 world is a "drought" — only rare 1-block puddles in sand bowls (a
screenshot confirmed it). ROOT CAUSE (verified in code + the 1.12 source):
the ONLY water mechanism is M11's heightmap fill ("fill air with water up to
`kSeaLevel`"), which fires only where terrain noise dips below sea level.
After M25 (sea 14→63, `height = kSeaLevel + 3 + base*18 + n*(3 + var*30)` in
`TerrainGen.cpp`), EVERY biome has a POSITIVE `base` (plains/desert 0.125,
forest 0.1, snowy 0.2) plus a +3 lift, so the floor sits ~y64 and terrain
almost never reaches y63. No biome dips, so no water.

VANILLA HAS TWO SEPARATE WATER SYSTEMS (both confirmed in the local source —
look up, don't recall):
1. OCEANS — biome+noise driven. Ocean biomes carry a NEGATIVE `baseHeight`:
   Ocean -1.0, Deep Ocean -1.8, variation 0.1 (`Biome.java:473/497`). Those
   negative bases drop whole regions far below sea level; the fill floods
   them. The clone has NO ocean biome and no negative-base biome, so it can't
   form an ocean basin.
2. LAKES — `WorldGenLakes` (`WorldGenLakes.java`), a POPULATE feature
   INDEPENDENT of the heightmap. Per chunk, skip desert, 1/4 water
   (`waterLakeChance=4`); lava ~1/80 and deep-biased
   (`ChunkGeneratorOverworld.java:412-430`). Walk down from a random y to
   ground, drop 4, carve a blob of 4-7 overlapping ellipsoids (≤16×8×16),
   fill the bottom 4 layers with liquid + air above — ONLY if the shell is
   SEALED (sides/bottom solid, no liquid in the upper half). Rim dirt→grass;
   freeze top in cold biomes; lava lakes wrap exposed faces in stone. KEY
   PROPERTY: it digs and seals its OWN basin, so lakes appear on dry flat
   above-sea terrain regardless of sea level — these are the field/forest
   ponds the user remembers.

PART A — OCEANS (CODE COMPLETE 2026-06-14, all in `TerrainGen.cpp`):
- A low-frequency "continentalness" OpenSimplex field (`m_seed + 404`, freq
  0.0015 → ~660-block features) added next to the climate fields. `biomeAt`
  now checks it FIRST: raw value < `kDeepOceanLevel` (-0.72) → DeepOcean,
  < `kOceanLevel` (-0.50) → Ocean, else fall through to the temp/moisture
  `ClassifyBiome`. New `Biome::Ocean`/`Biome::DeepOcean` enum values;
  `ParamsFor` returns base/var -1.0/0.1 and -1.8/0.1 (vanilla
  `Biome.java:473/497`); `TreeChance` returns 0 for both.
- THRESHOLD TUNING: the continentalness distribution was measured directly
  (a throwaway probe over a 1500-block grid): thr -0.30 = 36% ocean (too
  much), -0.50 ~ 25%, -0.55 = 22%, -0.72 = 14%. Chose -0.50 / -0.72 ->
  ~25% total ocean, ~13% deep — plenty of water but land-dominant so there's
  room to build. To make oceans rarer/commoner, nudge `kOceanLevel`; basin
  DEPTH is the biome base height, independent of the threshold.
- The rest fell out of existing systems with NO new code: the 5x5
  `cellParams` blend smooths base across the threshold -> sloping coastlines;
  the beach band (`height <= sea+2 -> sand`) makes sandy shores AND sandy
  ocean floors (the floor sits below the band, so it reads `sandy`); M11's
  heightmap fill floods everything below `kSeaLevel` with `blocks::Water`
  (all source -> flush + stable); trees/plants already gate on
  `height > kBeachTop` so nothing grows underwater. Measured depths: shallow
  ocean floor ~y42-54, deep ocean ~y27-40 (gentest saw maxDepth 32 -> floor
  y31). NO render changes — water uses the existing transparent liquid pass.
- gentest gained (a long transect scan, since oceans can miss a fixed
  window): "oceans generate (open water over a sub-sea-level floor)", "ocean
  water sits on a solid floor (no breach)", "deep ocean basins form (water
  >= 18 deep)". The existing "carved air never touches worldgen water"
  invariant still passes — the analytic `CaveGen` ocean-breach test is
  sea-level-based and ocean-agnostic, so it handled the much-larger water
  volume unchanged.
- KNOWN LIMIT: spawn (`kSpawnPos`, GameApp) is still a naive drop at a fixed
  XZ — on an ocean seed you splash into the sea and swim ashore (harmless: no
  fall damage, water breaks the fall). A spawn search needs the spawn chunk
  loaded at EnterWorld (deferred, same as M25's note). Oceans in snowy
  climate don't freeze (no ice block — out of scope).

PART B — LAKES (CODE COMPLETE 2026-06-14, new files `LakeGen.h/.cpp`):
- A port of `WorldGenLakes` (4-7 overlapping ellipsoids in a 16x16x8 blob,
  bottom half liquid / top half air) as a deterministic chunk-pure populate
  step, called from `TerrainGenerator::Generate` AFTER ores and BEFORE trees/
  plants (vanilla order). Water and lava share the code (`blocks::Lava`
  composes with M26); rarity per chunk: water 1/3, lava 1/40.
- THE DETERMINISM TRICK (the crux — read before touching it): instead of
  vanilla's live-world seal check (order-dependent), leaks/seams are made
  IMPOSSIBLE BY CONSTRUCTION:
  1. Each lake anchors to ONE chunk's 16x16 footprint (NO jitter) and the blob
     only sets interior cells (1..14), so every lake keeps a >=1-column solid
     margin — adjacent chunks' lakes can never touch/overlap (the first bug:
     random jitter made neighbours overlap at different levels -> exposed
     faces).
  2. The blob anchors BELOW the LOWEST surface across its footprint
     (`blobMinY = minGround - 4`), so every non-blob neighbour of a water cell
     is below every column's surface -> provably solid. No per-cell seal check;
     depends only on the global `heightAt`, so all chunks agree.
  3. The blob is constrained to fit in ONE vertical chunk
     (`blobMinY/16 == (blobMinY+7)/16`), so a single chunk owns + writes the
     whole lake and can cave-reject it locally — no cross-Y-seam coordination.
- CAVE REJECT: a lake whose liquid band/floor intersects a carved cell is
  dropped (uses this chunk's `caves::CarveMask`, which covers the whole blob
  since it fits in one chunk). Closes the last leak source (water over a cave)
  — gentest's seal scan went 39 exposed faces -> 0.
- DECORATION VETO: `lakes::LakeMask` (XZ columns, chunk + 2 skirt, filled from
  the same +-1 origin enumeration every chunk runs) lets the tree + plant gates
  skip a pond's footprint, like `CarveMask` does for cave mouths. (Lakes had to
  move BEFORE decoration for this; placing them after left undermined plants —
  the second bug.)
- PERF: `lakes::Place` early-outs (after one `heightAt`) for vertical chunks
  not near the surface band, so only the surface chunk(s) per column pay the
  256-column min/max footprint scan — keeps streaming cheap (6 of 8 vertical
  chunks skip it).
- gentest: "lakes generate above sea level" + "lake liquid has no exposed air
  face (sealed + seam-consistent)" (the seal scan IS the seam test — a
  truncated lake would show a cut face). The lava-band check relaxed to "no
  cave lava leaks into the mid-depths (y11..50)" since surface lava LAKES
  (always >= y56) are now legitimate. To keep the suite fast the lake scan
  reuses the structural region and the ocean transect was trimmed to ~1.5
  wavelengths (gentest ~33s; the cost is cave replay, not lakes).
- KNOWN LIMITS: lakes centre on a 16-block (chunk) grid — scattered enough with
  the flatness + rarity gates that it doesn't read as gridded, but a future
  tweak could add a safe sub-chunk offset. A cave-rejected lake site can leave
  a small tree-clearing with no pond (the veto mask is filled before the cave
  reject, and neighbours can't see this chunk's caves) — rare, cosmetic. No
  freezing/ice, no rim grass conversion (vanilla extras, skipped).

VANILLA REFERENCE: `WorldGenLakes.java`, `ChunkGeneratorOverworld.java:
412-430`, `Biome.java:473/497`.

## M28 — slabs & stairs (CODE COMPLETE)

CODE COMPLETE 2026-06-15, awaiting user verification (build + gentest +
savetest pass; gentest now reports 54 block types). NO new world required
(worldgen unchanged — slabs/stairs are never generated, only crafted/placed).
Scope DECIDED with the user: materials = stone / cobblestone / planks /
sandstone; STRAIGHT stairs only (no auto-corner inner/outer shaping); two
matching slabs MERGE into the full base block. This is the milestone the
M23/M24 notes kept pointing at — it reuses the float model stream (M23) for
geometry and the per-cell meta layer (M24) for state, and finally does the
partial COLLISION that both deferred.

- BLOCKS (`Block.cpp`/`Block.h`, APPEND-ONLY ids after Obsidian):
  `StoneSlab/CobbleSlab/PlankSlab/SandstoneSlab` +
  `StoneStairs/CobbleStairs/PlankStairs/SandstoneStairs`. `ShapeDef` copies
  the base material's def (textures + hardness + tool class + pickaxe gating +
  sound), then sets `opaque=false`, `solid=true`, `slab`/`stairs`, clears
  `model`, and forces `drop=kDropSelf` (a stone slab drops a stone SLAB, not
  cobble). NO texture/atlas changes — slabs/stairs reuse `faceTiles`, so
  neither `gen_textures.py` nor `import_mc_assets.py` was touched.
  `BlockDef::slabBase` names the full block a slab merges into. New BlockDef
  flags: `bool slab`, `bool stairs`, `BlockId slabBase`.
- META (`Block.h` `namespace facing`): slab meta 0 = bottom, nonzero = top
  (`SlabIsTop`/`SlabBottom`/`SlabTopMeta`). Stair meta packs the horizontal
  facing in the low 3 bits (a BlockFace; max value 5) + bit 3 (8) = upside-down
  — `StairsMeta(face,top)`/`StairsFacing`/`StairsIsTop`. Facing = the placer's
  LOOK direction (vanilla BlockStairs uses getHorizontalFacing directly, NOT
  the Opposite that furnace/table fronts use); the tall back sits on the
  facing side.
- RENDER (`ChunkMesher.cpp`): slabs/stairs set `out.model[p]=1` so the per-cell
  model pass handles them. `EmitModelCell` branches: `BuildSlabBoxes` (1 box,
  bottom/top half) / `BuildStairBoxes` (slab half + a quarter on the facing
  side — mirrors vanilla `getCollQuarterBlock`). `ShapeBox` builds each box
  with AUTO-UVs (each face samples the slice of its tile the box occupies, in
  the face's (u,v) basis, v up) so the texture reads as if cut from a full
  block. `EmitModelBox` gained `cullBoundary`: an axis-aligned box face lying
  exactly on a cell boundary (0/16 on its normal axis) is dropped when the
  neighbor that way is opaque — kills the z-fight of a slab's bottom on the
  ground or a stair's back on a wall. It's a no-op for the torch (inset planes,
  never on a boundary) and is OFF for oriented/rotated boxes. Lighting is still
  the cell's own light + full-bright AO (the model-stream default) — a known
  flatness limit on big slab faces (backlog: per-face light sampling).
- COLLISION (the real new work): `World::CollisionBoxesAt(wx,wy,wz, BlockBox
  out[2]) -> int` returns cell-local (0..1) AABBs — 0 for non-solid, 1 for a
  full cube or slab half, 2 for a stair (matching the render boxes exactly).
  `Player::MoveAxis` was rewritten to iterate these boxes (was: every solid
  cell = a full unit cube) and now does a PERPENDICULAR-axis overlap test
  before resolving (a half-slab box in a cell shouldn't block a move that
  passes above it). It returns `bool collided` now.
  - DIRECTIONAL GATE (critical fix — the user reported being "popped back to
    the start of a staircase" every ~0.5s while descending, and an earlier
    "jerk back" when brushing a stair's side): a box only clamps the move when
    it lies AHEAD of the player's pre-move LEADING face on the move axis
    (vanilla `AxisAlignedBB.calculateXOffset` semantics — `MoveAxis` computes
    `preLead` from `m_position[axis] - delta`). WHY IT MATTERS: the player AABB
    is 0.6 wide but a stair tread is a 0.5 half-cell, so standing on a stair
    you ALWAYS straddle the tall quarter-box beside your feet. Without the gate,
    moving AWAY from that box still resolved against it and shoved you to
    `bMax + extent` — backward, up the stairs. The gate skips boxes you already
    straddle on the move axis, so only boxes genuinely in your path block you.
- AUTO-STEP (`Player::TickWalk`): without this you'd have to jump onto every
  slab. After the normal grounded horizontal move, if it was blocked, retry the
  same move lifted by `kStepHeight=0.6` (vanilla) then settle back DOWN by the
  amount actually lifted (clamped by ceilings); keep it only if it advanced
  farther horizontally (so a full wall still blocks — 0.6 < 1.0 — but a
  slab/stair lip lets you walk up). Horizontal momentum is preserved across the
  step. CONTINUOUS-FOOTING GATE (fix after the user reported being "jerked back
  to the start of a staircase" when a jump/fall brushed its side): the step
  requires grounded BOTH this tick and last tick (`m_wasGrounded`), so the
  landing frame of a jump/fall can't trigger a climb — only genuine ground
  walking into a lip does. (Vanilla Entity.move steps on the landing frame too;
  we're deliberately stricter because preserved momentum + every-tick re-fire
  made a glancing touch rocket the player up the staircase.)
- PLACEMENT (`GameApp` place path, rewritten to compute placePos/Id/Meta then
  one SetBlock): `RaycastHit` gained `glm::vec3 point` (exact hit, computed
  from the DDA's entry t). `PlaceTopHalf(normal, point)` = vanilla rule (top
  face→bottom, bottom face→top, side face→the half you clicked). Slab meta from
  that; stair meta = `StairsMeta(HorizontalFromLook(forward), PlaceTopHalf)`.
  DOUBLE-SLAB MERGE: clicking the matching slab on its open face (top of a
  bottom slab / bottom of a top slab) sets that cell to `slabBase` (full block)
  instead of placing a second slab in the neighbor.
- CRAFTING (`Crafting.cpp`): per material, `MMM` → 6 slabs and the stair
  triangle (`M  `/`MM `/`MMM`, mirror auto-tried) → 4 stairs. Starter kit
  (`GameApp::EnterWorld`) seeds inventory slots 12-19 with all 8 shapes for
  instant testing (slots 9-11 are still the M26 buckets).
- PALETTE: all 8 appear in the creative palette automatically (non-liquid
  blocks). ICON (follow-up fix after the user grabbed a plank slab thinking it
  was planks — slabs/stairs reuse the base texture, so a flat full tile was
  indistinguishable from the full block): `DrawItemStack` (ui/Widgets.cpp) now
  draws a SHAPED icon for `def.slab`/`def.stairs` — a half-height tile for a
  slab, a 2-step silhouette for a stair — so they read as what they are in the
  hotbar, inventory, AND palette. (Dropped-in-world item entities still render
  as a full mini cube — transient, low-confusion, left as-is.)
- KNOWN M28 LIMITS / decisions: NO stair auto-corner shapes (straight only —
  corners leave the vanilla gap); selection outline + crosshair raycast still
  treat a slab/stair cell as a FULL cube (you can target the empty half) —
  tight boxes are backlog; model-block faces are flat-lit (own-cell light);
  placing a slab in the cell you stand in is blocked by the full-cell
  `Player::Intersects` check (minor); merged double-slabs drop the full base
  block (a stone double-slab drops stone, which then needs a pickaxe and drops
  cobble — minor economy quirk of merging into the existing block rather than
  a dedicated double-slab id); LOD still skips model blocks (worldgen makes
  none). Vanilla refs: `BlockStairs.java` (geometry/meta/placement),
  `BlockSlab.java`.

WHAT THE USER SHOULD TEST (NO new world needed — press E, grab from slots
12-19, or craft): place a slab on top of a block → bottom slab; aim at the
underside of a block → top slab; aim at the lower vs upper half of a block's
SIDE → bottom vs top slab. Place a second matching slab onto the open face of
a slab → it MERGES into a full block (different materials don't merge). Stairs:
the tall back points where you look; click a bottom face / lower half → upside-
down stair. WALK onto a slab or up a flight of stairs → you step up smoothly
(0.6) without jumping; a full block still needs a jump. Stand on a slab/stair
and confirm you don't sink or float. Check textures read continuously (sandstone
slab top/side, etc.) and there's no z-fighting where a slab meets the ground or
a stair backs onto a wall. Craft: 3 stone in a row → 6 stone slabs; the stair
triangle of planks → 4 plank stairs (both orientations). Quit + reload → placed
slabs/stairs keep their half/facing (meta persists).

## M29 — 3D block icons (CODE COMPLETE)

CODE COMPLETE 2026-06-15, user-verified the look ("looks good"; stairs facing
fixed in the same pass). NO new world needed (cosmetic — inventory/HUD only).
Before this, item icons were FLAT texture tiles (`DrawAtlasTile`); vanilla
renders block items as a live 3D iso model. We match that by BAKING an icon
sheet once and blitting cells through the existing 2D path — no per-frame 3D
draws interleaved with the UI batch, so the immediate-mode UI is untouched.

- HOW VANILLA DOES IT (looked up in the 1.12 source — see the resource note up
  top): `RenderItem.renderItemModelIntoGUI` → `setupGuiTransform` (translate to
  slot, scale 16, flip Y) + the model's `display.gui` transform from
  `models/block/block.json` (`rotation [30,225,0]`, `scale 0.625`) +
  `RenderHelper.enableGUIStandardItemLighting` (two fixed directional lights).
  Block items render as the 3D model; FLAT items (tools, sticks) use a
  "generated" sprite model — those genuinely are just the texture, so our old
  flat path was already vanilla-correct for them.
- ENGINE (new, reusable): `vox::Framebuffer` (renderer/Framebuffer.{h,cpp}) —
  an FBO with a color `Texture2D` + depth renderbuffer; `Bind()` sets itself +
  viewport, `Unbind()` returns to the window FB (caller resets viewport). New
  `Texture2D(w,h)` render-target ctor: single-level RGBA8, NEAREST,
  CLAMP_TO_EDGE, no mipmaps. Added to engine/CMakeLists.txt.
- GAME (`game/src/ui/BlockIcons.{h,cpp}` + `assets/shaders/block_icon.{vert,
  frag}`): bakes a grid sheet (12 cols) of every SOLID block as the iso model.
  Builds three shared meshes in the cube vertex layout (pos/normal/uv/face):
  full cube (0..1), slab (0..0.5 y), stair (bottom half-slab + a quarter step on
  the +Z half — under the 225° yaw that puts the riser at the back and the step
  toward the viewer). `EmitBox` mirrors the mesher's `kFaces`/`ShapeBox` UV rule
  (each face samples the tile slice its box occupies, v up), so a slab side
  shows the tile's lower half and textures match the world. The frag shader
  applies vanilla's per-face shade by normal (up 1.0, down 0.5, N/S 0.8, E/W
  0.6) — that's what makes a flat-colored cube read as 3D. MVP =
  `ortho * T(cellCenter) * S(cellPx) * S(0.625) * Rx(30) * Ry(225) * T(-0.5)`;
  y-up ortho + bottom-left FBO matches `DrawImage`'s stored-bottom-left sampling
  so icons come out upright (NO extra flip needed).
- WIRING: `GuiTextures` gained `const BlockIcons* blockIcons`. `DrawItemStack`
  (ui/Widgets.cpp) takes an optional `const BlockIcons*`: if the id `Has3dIcon`
  (block, not air, not a sprite/cross/torch, not a liquid flow level 1..7) it
  blits the sheet cell at 16*s (1:1, crisp); else the FLAT path (items, plants,
  and — when no icons are passed — the M28 slab/stair half-tile silhouette,
  which is now dead code in-game but kept as the no-icons fallback). All
  `DrawItemStack` call sites in Hud.cpp + InventoryScreen.cpp pass
  `tex.blockIcons`. GameApp owns `m_blockIcons` (built right after the atlas),
  sets `m_guiTextures.blockIcons`, and calls `EnsureBuilt(16*GuiScale, w, h)` at
  the top of `DrawUi` before `m_ui->Begin` — re-bakes ONLY when the GUI scale
  (window size) changes, so icons stay pixel-crisp at 1:1.
- KNOWN M29 LIMITS / decisions: dropped-in-world item entities + the
  first-person hand still use their own 3D cube path (unchanged, they already
  looked 3D); the icon sheet wastes a few cells (indexed densely, not by id);
  liquids (water/lava sources) render as plain cubes in the palette; the
  per-face shade is the EnumFacing constant, not vanilla's exact two-light GUI
  setup (visually equivalent, simpler). Slab/stair icons now use real partial
  geometry, retiring M28's faked silhouettes in-game.

WHAT THE USER SHOULD TEST (NO new world): E → palette/hotbar/inventory block
icons are 3D iso cubes; tools/sticks/buckets/plants stay flat; slabs read as
half blocks and stairs show the step facing you; sandstone/log/grass show the
correct top-vs-side textures; counts + tool durability bars still draw on top;
resize the window (GUI-scale change) → icons stay crisp, not blurry.

## M30 — health, damage & hunger (DONE)

DONE 2026-06-15, USER-VERIFIED (health/death/lava all confirmed in-world; the
hurt-feedback + real fire-overlay follow-ups verified "looks good"). First of
the survival & mobs arc (see the Planned arc section). Self-contained: HUD-only rendering, no
new entity/model renderer, no mobs. All values ported from the 1.12 source
(FoodStats.java, EntityLivingBase.attackEntityFrom/fall, GuiIngame.java) — look
them up there before changing tuning. NO new world needed (gameplay only).

- VITALS ON PLAYER (game/src/Player.{h,cpp}): health 0..20 (2 = one heart),
  foodLevel 0..20, a hidden saturation buffer (drains before food), exhaustion
  0..40, air -20..300 (300 = 15 s breath), a hurt-resist window, a fire timer,
  and accumulated fall distance. Ticked once per tick in `Tick()` — but ONLY in
  Walk mode. FLY MODE IS CREATIVE: no damage, no hunger, air pinned full, fall
  distance zeroed (so toggling fly never hurts you). `Dead()` latches at health
  0; GameApp reads it.
- DAMAGE (`ApplyDamage`, vanilla attackEntityFrom): respects the hurt-resist
  window (maxHurtResistantTime 20; a bigger hit during the window tops up to the
  new amount, an equal/lesser one is ignored), so e.g. lava lands ~twice/sec not
  every tick. Sources, all in `TickVitals`/`TickWalk`:
  - FALL: vanilla `updateFallState` — accumulate descent while airborne, on
    landing deal `ceil(distance - 3)`. Water cancels it. The FIRST landing after
    any `Teleport` is exempt (`m_spawnFallGrace`) so the spawn drop (spawn is
    above the surface) and respawn never kill you.
  - LAVA: 4/tick + sets a 15 s fire timer; FIRE: 1 dmg/sec while the timer runs
    (keeps burning after you climb out — vanilla). WATER extinguishes the fire
    timer instantly (vanilla `extinguish()` — dive in to escape a post-lava
    burn). The real vanilla FIRE OVERLAY shows the burn: `blocks/fire_layer_1`
    (16x512 = 32 frames of 16x16, imported standalone to
    `mc/textures/fire_layer_1.png` via the COPIES list; loaded as
    `m_fireOverlay`) tiled across the bottom of the view in DrawUi, animated off
    `m_totalTicks` with desynced columns, alpha 0.9 (matching
    ItemRenderer.renderFireInFirstPerson). Gated on `Player::OnFire()` and not
    eye-in-lava (the lava tint wins when submerged). No-asset clones fall back
    to a flickering orange tint. DROWNING: head in WATER drains air 1/tick, then
    2 dmg/sec once it bottoms out. VOID: 4/tick below y -64.
  - CACTUS: 1/tick while touching. NOTE: our cactus is a full-cube collision
    (the vanilla 14/16 inset is still backlog), so the player can't truly
    overlap the cell — the touch test grows the AABB 0.1 horizontally so
    pressing against a cactus hurts, with the vertical range inset so standing
    on top is safe. (Proper fix = the inset collision box, backlog.)
- HUNGER (`TickFoodStats`, exact FoodStats.onUpdate port, naturalRegeneration
  on, normal difficulty): exhaustion ≥ 4 spends 1 saturation (then food); fast
  regen while saturated + full, slow regen at food ≥ 18, starvation at food 0
  (normal: floors at 1 health, never kills). Exhaustion drivers: movement
  (sprint 0.1/block, walk 0.01/block — in `TickVitals`), jump (0.05, sprint
  0.2 — in `TickWalk`), block break (0.005 — in `GameApp::HandleInput`).
- HUD (game/src/ui/Hud.{h,cpp}): `HudVitals` struct → `DrawVitals` draws hearts
  (from the hotbar's left edge), food (anchored right), and air bubbles (while
  submerged) on the existing icons.png sprite path, laid out exactly like
  GuiIngame (9x9 icons; heart bg/full/half at u 16/52/61 v 0, food at v 27,
  bubbles at u 16/25 v 18). `show` is false in fly/creative so the bars hide
  (vanilla). Needs the icons.png overlay (placeholder builds skip the bars).
- DEATH / RESPAWN (GameApp): new `State::Dead`. `EnterDeathScreen` (called from
  OnTick when `Dead()` first trips) frees the cursor, returns any open
  container's carried/craft contents to the bag, freezes the player + world
  (Dead is neither Playing nor a container, so OnTick's sim block is skipped —
  the death scene holds still). DrawUi paints a red "You Died!" overlay with
  Respawn / Title Screen buttons. `RespawnPlayer` resets all vitals to full and
  teleports to the world spawn column, standing on the highest solid non-liquid
  block (falls from spawn height with fall-grace if that column isn't streamed
  in yet). Esc/E are inert in Dead (their existing guards no-op).
- PERSISTENCE (WorldSave): a new optional manifest line `vitals <health>
  <foodLevel> <saturation> <exhaustion> <air>`. Absent in pre-M30 saves → full
  health on load (the `player`/`time`/`inventory` lines are untouched, so old
  saves stay bit-compatible). Loaded in `EnterWorld` (→ `Player::SetVitals`,
  which clamps), written in `PersistPlayerState`. savetest covers absence +
  exact round-trip.
- HURT FEEDBACK (follow-up, same milestone): the vanilla damage NOISE + camera
  TILT (ported from EntityRenderer.hurtCameraEffect / EntityLivingBase). The
  engine `PerspectiveCamera` gained a `SetRoll`/`Roll` (degrees about the view
  axis; View() rolls the up vector around forward) — independent of
  SetRotation so a transient tilt layers on top. `Player` tracks `m_hurtTime`
  (set to 10 on a fresh hit in `ApplyDamage`, decremented each Tick) and exposes
  `CameraRoll(alpha)` = `sin(f^4·π)·14°` with `f = (hurtTime - alpha)/10` (the
  exact vanilla curve; attackedAtYaw is 0 for environmental damage so it's an
  undirected roll — directional knockback waits for M32 mobs). GameApp applies
  it each frame in OnRender right after `m_player.OnRender`, and adds the death
  keel-over `40 - 8000/(deathTicks+200)` driven by `m_deathAnim` (seconds in the
  Dead state). The hurt SOUND: `Player::ConsumeHurt()` returns true once per
  fresh hit (only on a full hit, not a within-resist-window top-up — vanilla);
  GameApp plays `GameSounds::PlayHurt` (2D, pitch 1.0 ± 0.1) from
  `damage/hit{1,2,3}.ogg` (the import script's sound families grew `damage/`;
  re-run it to pull them — 130 sound files now). A clean clone with no overlay
  stays silent as before.

WHAT THE USER SHOULD TEST (NO new world): hearts + hunger bar appear above the
hotbar in walk mode and HIDE in fly mode (press F); fall ~4+ blocks in walk mode
to lose hearts (a big fall kills → "You Died!" screen → Respawn drops you back
at spawn at full health, Title Screen quits to the menu); standing in lava
drains health fast and you keep burning briefly after stepping out; dive
underwater and watch the air bubbles pop, then take drowning damage once they're
gone; walk into a cactus to take a tick of damage; sprint/jump/dig for a while
to watch hunger tick down, then watch health slowly regen while fed; the empty-
handed/low-hunger states regen correctly; quit + re-enter → health/food
restored; an OLD world loads at full health and plays normally. HURT FEEDBACK
(re-run `import_mc_assets.py` first for the sound): each hit plays a grunt and
snaps the view into a quick tilt that eases back; on death the view slowly keels
over behind the "You Died!" screen.

## M31 — entity model renderer (DONE)

DONE 2026-06-16, USER-VERIFIED ("works perfect" — debug Steve walks the circle,
limbs swing, skin maps, orientation correct). The shared bottleneck of
the survival & mobs arc: a reusable jointed box-model renderer. NO gameplay yet
— it ships on a debug entity (press G) so the renderer + animation can be
verified before M32 mobs / M33 player-doll consume it. NO new world needed.
IMPORTANT: the debug Steve only draws with the gitignored mc asset overlay
present (it loads `mc/textures/entity/steve.png`, the same skin M20's
first-person arm uses) — re-run `import_mc_assets.py` if a fresh pull shows
nothing on G. A clean clone without the overlay silently draws no mob (like the
bare-hand arm), which is fine.

- ENGINE: `vox::BoxModel` (engine/src/vox/renderer/BoxModel.{h,cpp}) — a jointed
  multi-cuboid model in the spirit of vanilla `ModelBase`/`ModelRenderer`.
  `AddPart(name, pivot, boxes, parent)` registers a named part (cuboid group)
  with a rotation point and an optional parent (children inherit the parent
  transform; parents must be added first). Each `Box` is an origin+size in
  PIXELS (part-local, relative to the pivot — matching `ModelRenderer.addBox`),
  a `texOffset` into the skin, an `inflate` (vanilla "delta", e.g. the hat
  overlay's 0.5), and a `mirror` flag (reflects across X so a left limb reuses
  the right's UV island). `Build()` bakes one VAO per part with the classic
  vanilla box UV unwrap (the six `TexturedQuad` islands laid out around
  `texOffset`, ported verbatim — see `AppendBox`). The renderer is animation-
  AND unit-AGNOSTIC: it does no scaling/flip of its own. The caller sets each
  part's rotation (`SetRotation`, radians, vanilla Y-down local frame, applied
  Z→Y→X), binds a shader + sets view/proj + lighting, then calls `Render(shader,
  modelToWorld)` — which sets `u_model` per part (modelToWorld × accumulated
  local) and binds the skin to unit 1. `modelToWorld` maps pixel/Y-down model
  space to world space, folding in the 1/16 scale + the upright flip; both the
  in-world mob path and the future UI doll reuse this with different matrices.
- SHADERS: `assets/shaders/entity_model.{vert,frag}` — pos/normal/uv in, per-
  part `u_model`, `sampler2D u_skin`. Same lighting model as `block_entity.frag`
  (one sampled sky/block light for the whole entity, sun/moon diffuse, no fog)
  + an alpha discard < 0.5 so transparent skin-overlay layers (the hat) cut out.
  Game-owned files loaded by GameApp; `BoxModel` stays GL-clean and takes the
  bound `Shader&`.
- GAME: `vc::HumanoidModel` (game/src/HumanoidModel.{h,cpp}) — builds the Steve
  biped on a `vox::BoxModel`: head (+hat overlay), body, R/L arms, R/L legs,
  authored from ModelBiped's exact pixel boxes/pivots/texOffsets (left limbs use
  `mirror`), skin = `mc/textures/entity/steve.png` (64×64). `SetRotationAngles`
  is a verbatim `ModelBiped.setRotationAngles` port (limb swing on the walk
  cycle at 0.6662 freq, arms/legs ±π out of phase, + the constant idle arm
  sway; head tracks yaw/pitch in degrees). `Ready()` is false without the skin.
- DEBUG ENTITY (GameApp): `m_debugMob` (a `DebugMob` struct — active flag,
  circle center, prev/current pos+yaw, vanilla `limbSwing`/`limbSwingAmount`
  accumulators with prev for interpolation, age). G toggles it (`ToggleDebugMob`,
  Playing only): spawns ~4 blocks ahead, then `TickDebugMob` (20 TPS) paces a
  2.5-block circle (~3 b/s), faces its travel direction, scans down each tick to
  stand the feet on the surface, and feeds the walk-cycle accumulators from its
  horizontal speed (vanilla `EntityLivingBase.onLivingUpdate`). It is render-
  interpolated, drawn in the opaque slot of OnRender (between the entity-cube
  pass and the water pass) via the model matrix `T(pos)·Ry(π/2−yaw)·S(1/16)·
  T(0,24px)·Rx(π)` (the Rx(π)+24px flip turns vanilla's Y-down, feet-at-y24
  model upright with feet at the entity base; the model faces +Z after the flip,
  hence the π/2−yaw body yaw). Light sampled at the body cell like the cubes.
  NOT persisted, NO collision, NO gameplay — purely a renderer test rig.
- Known M31 limits / notes: only the head HAT overlay layer is included (body/
  arm/leg second layers are 1.8+ "skin overlay" boxes — backlog); the mirror
  path U-flips islands across X but doesn't swap the east/west islands the way
  vanilla does (visually identical on the symmetric Steve skin); the debug mob
  ground-scan is a simple column probe (the small circle can clip a steep
  hillside — it's a debug rig, not a real entity). If the model renders upside-
  down or facing the wrong way on the user's first look, the fix is a sign flip
  in that one `modelToWorld` (the Rx(π) / the π/2−yaw term) — called out because
  per the working agreement the agent didn't run the game to confirm orientation.

WHAT THE USER SHOULD TEST (NO new world; re-run import_mc_assets.py first so the
Steve skin is present): enter a world, press G — a Steve should appear ~4 blocks
in front, standing on the ground, and walk a small circle with arms and legs
swinging in opposite phase (plus a subtle idle arm sway) and the body turning to
face the way it walks; the skin should map correctly (face on the front of the
head, no scrambled UVs); it should be lit by the world (darker in shade/at
night); press G again to remove it. Confirm orientation: it stands upright (not
upside-down or sunk into the ground) and faces its direction of travel. (It has
no collision and isn't saved — that's expected for the debug rig.)

## M32 — mobs (AI, spawning, combat) (CODE COMPLETE)

CODE COMPLETE 2026-06-16, awaiting user verification. Builds on M31's BoxModel
renderer + M30's vitals + the existing FallingBlock/ItemEntity entity pattern.
Decisions confirmed with the user before coding: zombie DAYLIGHT BURNING is
deferred to backlog (kept M32 focused); FULL combat feedback (player melee +
knockback, mob red hurt-flash + hurt/death sounds, zombie melee → directional
player knockback). NO new world needed (gameplay only).

- DATA MODEL (`game/src/world/Mob.{h,cpp}`): `MobType{Pig,Zombie}` + a
  `kMobDefs` table (half-width/height, maxHealth, speed, hostile, attackDamage,
  followRange, model feet-offset, drop count range). `Mob` is the same
  tick-simulated, prev/current-interpolated, AABB struct as FallingBlock/
  ItemEntity: pos/prevPos/vel, body yaw, health, onGround, fallDistance, the
  vanilla walk-cycle accumulators (limbSwing/limbSwingAmount + prev, age), plus
  hurtTime/attackCooldown/aiTimer/wanderDir. Pig 0.9×0.9 / 10 hp; zombie
  0.6×1.95 / 20 hp / attack 3 / follow 16. `MobDropItem` returns the runtime
  item id (pig → raw porkchop, zombie → rotten flesh).
- SIMULATION (`World::TickMobs`, Mob.cpp): called from GameApp::OnTick in the
  same Playing/ContainerOpen block as the player/world tick. World stays Player-
  and audio-agnostic — it takes a `MobTickCtx{playerFeet, playerHalfWidth/Height,
  isNight, damagePlayer(dmg,fromPos), pushPlayer(dx,dz)}`. Per mob: freeze if the
  feet chunk isn't loaded (like the player/sand); AI → wish dir + facing; gravity
  + axis-separated block collision with a 1-block auto-step (`MoveMobAxis`,
  mirrors Player::MoveAxis over `World::CollisionBoxesAt`, full step-and-settle
  with the same gain check so mobs don't climb walls); walk-cycle accumulators
  (copied from the M31 DebugMob); soft entity push out of the player's body
  (`pushPlayer` nudges the player via Player::ExternalPush, resolved against
  blocks); hostile melee; fall/void death. AI: pig wanders (random stroll dir,
  2–5 s, with idle pauses) at 0.55× speed; zombie within followRange targets the
  player, walks straight at them (simple ground pathing; auto-step climbs
  1-block rises), and on contact (`attackCooldown<=0`) calls `ctx.damagePlayer`
  and resets the ~1 s cooldown. Knockback (set by DamageMob) decays through the
  wish each tick via ground friction.
- SPAWNING / DESPAWN (`World::SpawnMobs`, every ~2 s): counts mobs per category
  within 64 blocks (caps 8/8), then a few attempts pick a ring position 24–44
  blocks out, scan for ground (solid non-liquid below, 2 air above, only if the
  chunk is loaded), and gate by light/surface: pig on `Grass` with sky-light ≥ 9
  in daytime, zombie where block-light ≤ 7 and (`isNight` or sky-light ≤ 7).
  Hostiles farther than 54 blocks despawn; passives persist. `isNight` is
  GameApp's `IsNight()` (sun elevation < 0, same phase as ComputeDayNight).
- COMBAT (`Player` + GameApp): M30 left directional knockback for this
  milestone. `Player::ApplyDamage` now returns whether a FRESH hit landed;
  `Player::Hurt(amount, fromPos)` wraps it to add a decaying horizontal
  knockback (+ a small upward pop, applied additively over the wish velocity in
  TickWalk) and a directional `CameraRoll` (the hurt tilt now leans by which
  side the hit came from; environmental damage stays undirected). Player melee
  (GameApp::HandleInput, LMB press-edge): `World::RaycastMob` (slab test vs mob
  AABBs); if a mob is in reach (5) and nearer than the targeted block, it's
  attacked instead of digging — bare hand 2.0, +tier+1 holding an axe (no sword
  item exists yet — tunable). `World::DamageMob` applies health + red-flash
  hurtTime + knockback away from the attacker + a hurt/death `MobSound` event;
  drops loot and removes the mob at ≤ 0.
- RENDERING (GameApp::OnRender mob pass, in the opaque slot next to the debug
  Steve): the zombie reuses `HumanoidModel` (now takes a skin path + a
  zombie-arms-forward pose flag) with `entity/zombie/zombie.png`; the pig is the
  new `PigModel` (quadruped, ported from vanilla ModelQuadruped/ModelPig — head
  with a snout, horizontal body, four short legs; 64×32 skin). One shader bind,
  then per mob: interpolate pos/yaw/limbSwing, sample body-cell light,
  `u_hurt` = hurtTime>0 (the entity_model.frag now blends toward red), and the
  same upright-flip matrix as the debug Steve (the per-type feet offset is 24px
  for both). Each model silently skips without its skin overlay (clean clone).
- AUDIO (`GameSounds`): mob voices loaded from `mob/pig/` + `mob/zombie/`
  (say/hurt/death sets; pig hurt falls back to its "say" set, vanilla has no pig
  hurt clip). `PlayMobHurt/PlayMobDeath(hostile, pos)` are positional; GameApp
  drains `World::MobSoundEvents()` after the mob tick (keeps World audio-free
  like ForEachLitFurnace). The player's own grunt still rides M30's
  ConsumeHurt → PlayHurt (now also checked after the mob tick).
- PERSISTENCE: a `mobs.dat` sidecar mirroring furnaces.dat
  (`WorldSave::MobRecord{type,pos,yaw,health}`, Get/SetMobs, ReadMobs/WriteMobs,
  debounced Flush). World loads mobs in its ctor (unknown/newer types dropped)
  and `SaveMobs()` rides SaveEditedChunks (autosave + ~World). savetest grew a
  round-trip case (zombie/pig records + empty-set file deletion). An old save
  with no mobs.dat loads with an empty world.
- TESTABILITY: natural spawns aside, debug keys spawn a real mob ~3 blocks
  ahead — `B` = pig, `C` = zombie (`GameApp::SpawnMobAhead`). G's debug Steve
  (M31) is untouched.
- Known M32 limits / decisions: soft entity push is mob-side (the mob shoves
  itself out and nudges the player via ExternalPush) rather than hard player↔mob
  block-style collision — you can still pass through a mob with a shove, no
  getting stuck (a hard push is backlog); zombies don't burn in daylight
  (deferred); no sword/attack-cooldown-bar, no baby/variant mobs, no breeding/
  feeding, pig/zombie only; pathing is straight-line + auto-step (no A*), so a
  zombie can hang up on a 2-block wall or a deep hole; mob drops aren't affected
  by the food/eating system (porkchop/rotten flesh are sprite-only items — no
  eating yet, that's backlog with the rest of farming/food).

- POST-SCREENSHOT TWEAKS (2026-06-16, from the user's first look): the pig body
  pivot Y was lowered 17 → 12 so the body sits ON the legs instead of dropping to
  the ground with the legs buried inside it (our upright flip maps
  world-y = 24 − (pivotY + rotatedBoxY), so vanilla's 17 lands the body too low;
  this also tucks the head down onto the body). The zombie chase speed was
  dropped 4.4 → 4.0 b/s — just under the player's 4.3 walk, matching vanilla's
  "slightly slower than walking" (movementSpeed 0.23): you can now walk away
  slowly and sprint away easily, where 4.4 meant only sprinting worked. Mob
  horizontal movement also gained an ACCELERATION RAMP (`kAccel` 14 b/s² in
  TickMobs): instead of snapping velocity to wish*speed each tick, it steps
  toward the target by a capped amount, so mobs have momentum + reaction lag
  (a few ticks to reach speed, can't instantly reverse to track a dodging
  player) and knockback bleeds off through the same ramp (no special case).

WHAT THE USER SHOULD TEST (NO new world; re-run `import_mc_assets.py` first for
the skins + mob sounds): press `B` for a pig — it should stand on the ground and
amble around in random directions, legs trotting, body turning to face its path;
press `C` for a zombie — it should turn toward you and walk straight at you,
arms out, and when it reaches you deal damage that knocks you back + tilts the
camera from the hit side + plays the hurt grunt. LMB-click a mob (in reach, with
nothing closer) → it flashes red, takes knockback, and after a few hits dies
with a death sound and drops porkchop / rotten flesh that you can pick up (watch
the hotbar). Holding an axe kills faster. Let the clock run: pigs should appear
on grass in daylight, zombies at night (use T to fast-forward); wander a long
way from a spawned zombie and it should despawn. Quit + re-enter the world → the
mobs you left are restored from mobs.dat (pigs persist; far hostiles may have
despawned). An OLD world (no mobs.dat) loads fine with an empty mob set.

## M33 — armor & the inventory player doll (DONE & USER-VERIFIED)

DONE & USER-VERIFIED 2026-06-17 ("the character model is facing in the right
direction, armor works, everything looks good"). Finishes the survival arc:
now that mobs hit you (M32), armor mitigates it, and the player character shows
in the inventory. Decisions confirmed with the user before coding: FULL vanilla
armor set (5 materials × 4 slots), the worn-armor LAYER rendered on the doll
(not body-only), and vanilla 1.12 armor-points math. Built on M19 items/
durability, M30's damage path, M31's box-model renderer, and M29's framebuffer→UI
path — almost entirely wiring + data, one new engine capability (per-part model
visibility).

- ITEM DATA (`game/src/item/Item.{h,cpp}`): `ItemDef` grew `armor`, `armorSlot`
  (`ArmorSlot{Head,Chest,Legs,Feet}` = 0..3), `defensePoints`, `armorToughness`,
  `armorTexture` (the material key, e.g. "iron"). `RegisterDefaults` appends 20
  pieces from a material×slot table (sprite tiles 71..90, material-major) using
  vanilla `ItemArmor.ArmorMaterial` values: durability = MAX_DAMAGE_ARRAY[slot]
  ({11,16,15,13} for head/chest/legs/feet) × the material factor (leather 5,
  chain/iron 15, gold 7, diamond 33); defense points + toughness (only diamond
  2.0) straight from the enum. `FirstArmor` + `ArmorPiece(material, slot)`
  address a piece without 20 named externs; `IsArmor/ArmorSlotOf/ArmorDefense/
  ArmorToughness/ArmorTexture` are the queries. The creative palette auto-lists
  them (it enumerates the whole item registry).
- ASSETS (`import_mc_assets.py` + `gen_textures.py`): both atlas scripts grew
  the SAME 24 tiles in the SAME order — 20 armor icons (71..90) + 4 empty-slot
  placeholders (91..94, Head/Chest/Legs/Feet), so the texture array is 95 layers
  now (was 71). Leather icons are grayscale base × the default un-dyed color
  (0xA06540) composited with the untinted overlay. The import COPIES the worn
  model-layer textures (`models/armor/{mat}_layer_{1,2}.png`) and RE-BAKES the
  two leather layers tinted (same as the icons). RE-IMPORT REQUIRED on this
  machine for the real icons + doll skins (a clean clone draws placeholder
  silhouettes + no doll, like the debug Steve).
- INVENTORY (`game/src/item/Inventory.{h,cpp}`): a separate
  `std::array<ItemStack, kArmorSlots> m_armor` with `Armor(slot)`/`ArmorSlots()`
  accessors (NOT part of the 36 main slots). Persisted on its own `armor2`
  manifest line (mirrors `inventory2`; absent in pre-armor saves → nothing worn);
  `GameApp::EnterWorld` loads it, `PersistPlayerState` writes it. savetest grew a
  worn-armor round-trip + the persists-across-inventory-rewrite case. A NEW world
  also gets a debug diamond set in grid slots 20..23.
- DEFENSE (`Player.{h,cpp}` + `GameApp::OnTick`): `ApplyDamage(amount,
  bypassArmor)` — `AbsorbArmor` runs vanilla `CombatRules.getDamageAfterAbsorb`
  (`f = 2 + toughness/4; clamp(armor - dmg/f, armor*0.2, 20); dmg*(1 - that/25)`)
  for armor-applicable sources. Per vanilla, FALL / burn-over-time (onFire) /
  drown / starve / void BYPASS armor; lava-touch / cactus / mob melee do NOT.
  `GameApp` sums the worn pieces' defense+toughness into `Player::SetArmorStats`
  before the tick; after combat it drains `Player::ConsumeArmorWear` (the raw
  absorbed damage) and wears each piece by `max(1, raw/4)` (vanilla
  `InventoryPlayer.damageArmor`), breaking pieces at their durability limit.
- DOLL RENDERER (`game/src/render/PlayerDoll.{h,cpp}`, new): bakes the body +
  worn armor to an offscreen `vox::Framebuffer` (M29 path), blitted into the
  inventory panel. Reuses `HumanoidModel`, now generalized: constructor takes an
  `inflate`, skin dims (armor layers are 64×32), and `includeHat`; `BoxModel`
  gained per-part `SetVisible` (the new engine capability). Armor layers are
  inflated bipeds per vanilla `LayerArmorBase`: layer 1 (+1.0, `{mat}_layer_1`)
  for helmet (head) / chest (body+arms) / boots (legs); layer 2 (+0.5,
  `{mat}_layer_2`) for leggings (body+legs). Models are built lazily per
  material + cached. The bake uses a y-up ortho (BlockIcons' upright trick) and
  flat full-bright lighting; it runs in `DrawUi` right after the block-icon bake
  (depth still on) only in `State::Inventory`, then restores the viewport.
- UI (`game/src/ui/InventoryScreen.{h,cpp}`): four armor slots down the left
  edge (vanilla x=8, y=8/26/44/62) — empty ones draw their placeholder sprite;
  `ClickArmorSlot` is a type-gated swap (only the matching `ArmorSlot` goes in,
  anything comes out — works for both buttons since armor stacks to 1). The doll
  texture (passed via `GuiTextures::playerDoll`) blits into the panel box
  (`kDollBoxSize` 50×64, baked at that × the GUI scale). Both the slots and the
  doll are player-screen only (`!table`); the crafting table + furnace screens
  are untouched.
- KNOWN M33 LIMITS / decisions: NO shift-click quick-equip (the game has no
  shift-click quick-move anywhere yet — equip by grabbing from the palette/
  inventory and clicking the slot); armor isn't a mob/chest drop yet (creative
  palette + the new-world debug set are the sources); no enchantments/
  enchantability; the doll is a static idle pose (no mouse-follow head like
  vanilla); armor durability, like the rest of the inventory, persists on quit
  only (not the 30 s autosave). The doll YAW (`PlayerDoll.cpp` `yaw = 200°`) +
  scale/position were user-confirmed as facing forward and well-placed.

WHAT THE USER SHOULD TEST (re-run `import_mc_assets.py` first; works in ANY
world — armor is in the creative palette, and a NEW world also spawns a debug
diamond set in the inventory grid): open E → the four armor slots show down the
left with empty-slot icons, and the player doll renders between them and the 2×2
craft grid. Grab armor from the palette (or the debug set) and click an armor
slot → it equips (and the doll shows the worn layer); a wrong-slot piece is
rejected; click a worn slot with an empty cursor → it comes off. Confirm the
doll FACES FORWARD and is sized/placed sensibly (flag if not). Take damage from a
zombie (C to spawn) with vs. without armor → armor visibly reduces the hit, and
pieces wear (durability bar on the icon) and eventually break. Confirm FALL /
drowning / starvation still hurt at full strength through armor (they bypass it),
while lava/cactus/mob hits are reduced. Quit + re-enter → worn armor restored
from the `armor2` line; an OLD world loads fine with nothing worn.

## M35 — explosion system → Creeper (+ TNT) (DONE & USER-VERIFIED)

DONE & USER-VERIFIED 2026-06-18 ("it all works"). Roadmap step 3: build the
shared EXPLOSION system once, then drive it from a Creeper mob and a primed-TNT
entity. Decisions confirmed with the user before coding: terrain destruction ON
(vanilla mobGriefing — explosions carve blocks, obsidian/bedrock survive); the
full content chain (gunpowder + flint & steel + craftable TNT); Creeper + TNT
both this milestone. Mechanics ported verbatim from the 1.12 source
(world/Explosion.java, EntityCreeper, EntityTNTPrimed, ModelCreeper). NO new
world needed. RE-IMPORT REQUIRED on this machine: `python
scripts/import_mc_assets.py` — M35 added 5 atlas tiles (TNT side/top/bottom
103..105, gunpowder 106, flint & steel 107 → texture array is 108 layers now),
the creeper skin (`entity/creeper/creeper.png`), and the creeper sound family +
the boom/fuse one-offs (199 sound files now: `mob/creeper/say1..4` + `death`,
`random/explode1..4`, `random/fuse`). A clean clone with no overlay draws no
creeper and is silent for the boom, like the M32 pig/zombie.

- THE SHARED ROUTINE (`EntityManager::Explode`, game/src/entity/Explosion.cpp —
  split out of EntityManager.cpp to stay within budget): a verbatim port of
  Explosion.doExplosionA/B. (1) BLOCK CARVE: 16×16 surface rays march from the
  center at 0.3 steps, strength `size*(0.7..1.3)` decremented 0.225/step and by
  `(blastResistance+0.3)*0.3` per non-air block; cells the ray still has strength
  at join a dedup set. `BlockDef::blastResistance` (Block.h, 0 = derive from
  hardness; stone/cobble 6, obsidian 1200) gates it; `unbreakable` (bedrock) and
  `liquid` blocks absorb the ray entirely (never carved). Each carved cell rolls a
  `1/size` drop then `SetBlock(Air)` (which auto-schedules neighbor updates → sand
  cascades + water backfill come free). (2) ENTITY DAMAGE (computed on the intact
  world, before removal, like vanilla): within `2*size`, falloff
  `d=(1-dist/2size)*density`, `dmg=(int)((d²+d)/2 * 7 * 2size + 1)`. `density` is a
  single center→target LOS raycast (1.0 clear, 0.4 behind cover — vanilla samples
  the full AABB; ours is the cheap approximation). Mobs go through `DamageMob`
  (knockback/flash/drops/erase; iterate-by-index, re-check after erase). The
  player goes through an injected `damagePlayer(dmg, center)` callback (reuses
  `Player::Hurt` knockback). (3) Queues an `ExplosionEvent{pos,size}` GameApp
  drains for the boom + debris puff.
- PLAYER-CONTEXT PLUMBING: World stays Player/audio/particle-agnostic. GameApp
  calls `EntityManager::SetExplosionTargets(playerFeet, damagePlayer)` before
  `World::Tick()` each tick (the TNT path detonates inside `Tick()`); the same
  callback is also the mob ctx's `damagePlayer` (creeper path, in `TickMobs`).
  Fly mode = creative, no damage (the callback gates on Walk, same as mob melee).
- CREEPER (a data row + an AI branch + a model): `MobType::Creeper` appended
  (=5, mobs.dat ids stay stable). `MobDef` grew `explodeRadius` (3.0) + `fuseTime`
  (30); `Mob` grew runtime `fuse`/`prevFuse`/`fuseLit` (NOT persisted, like
  sheared/eggTimer). Row: 0.3×1.7, 20 hp, ~3.4 b/s, hostile, attackDamage 0
  (it explodes, never bites), followRange 16, SpawnRule::Dark weight 100 (≈50/50
  with the zombie at night), drops gunpowder 0–2 (only if KILLED before it blows).
  TickMobs: when `explodeRadius>0` it chases like a hostile and swells — `fuse++`
  while `fuseLit` OR within ~3 blocks of the player (the 0→1 edge queues a prime
  hiss, MobSound kind 3), else `fuse--`; at `fuseTime` it pushes a deferred
  detonation + erases itself (no drop). Detonations run AFTER the TickMobs loop
  (Explode erases mobs, which would invalidate the loop index). `CreeperModel`
  (entity/CreeperModel.cpp) is a verbatim ModelCreeper port (8³ head, upright
  8×12×4 body, four 4×6×4 legs, 64×32 skin, `IMobModel` like CowModel).
- TNT (a block + a primed entity): `blocks::Tnt` (top/side/bottom tiles, hardness
  0, drops self, 0 blast resistance). `EntityManager::PrimedTnt` (a Body + fuse) —
  vanilla EntityTNTPrimed physics (gravity 0.04 b/tick², drag ×0.98, ground
  ×0.7/−0.5 bounce, AABB collision), ticked in `EntityManager::Tick()`; at fuse 0
  it `Explode(4.0)` + erases. NOT persisted (vanishes on quit, like dropped items
  — a rare edge case). Rendered in GameApp's entity-cube pass (block_entity
  shader) as a 0.98 cube that blinks bright (u_unlit, vanilla's `fuse/5 % 2`
  flash) near detonation.
- CONTENT + CRAFTING: `items::Gunpowder` (sprite) + `items::FlintAndSteel` (a
  64-use damageable igniter, mirrors Shears). Recipes (Crafting.cpp): TNT =
  gunpowder/sand checkerboard `GSG/SGS/GSG`; flint & steel = shapeless
  `{iron ingot, coal}` (a pragmatic substitute — no gravel/flint block exists,
  documented). The creative palette auto-lists all three.
- IGNITION (`GameApp::TryIgnite`, RMB chain beside TryShearSheep/TryUseBucket):
  holding flint & steel, a creeper in reach (nearer than any block) gets
  `IgniteMob` (sets fuseLit → swells to detonation regardless of range); else a
  targeted TNT block is removed + replaced with a `SpawnPrimedTnt` (80-tick fuse).
  Either path wears the tool one use + plays the fuse hiss.
- AUDIO (`GameSounds`): the per-MobType voice array grew to 6 (creeper
  hurt/death from `mob/creeper/`). `PlayExplosion` (random/explode*) +
  `PlayCreeperPrime` (random/fuse, pitch 0.5) added; GameApp drains
  ExplosionEvents → boom + `ParticleSystem::SpawnExplosion` (a radial debris
  burst textured from the ground under the blast), and MobSound kind 3 → prime.
- TESTABILITY: debug key K spawns a creeper ~3 blocks ahead (KeyCodes.h grew
  `Key::K`). TNT + flint & steel + gunpowder are all in the creative palette.
- KNOWN M35 LIMITS / decisions: primed TNT isn't persisted (gone on quit);
  creepers persist as mobs but their fuse/lit state resets on load (runtime-only);
  the blast-density LOS is a single ray, not the full-AABB sample (so cover
  reduces damage coarsely); a TNT block caught in a blast is just removed, it
  doesn't chain-prime (vanilla re-primes it — backlog); no charged/powered
  creepers (no lightning/weather); the creeper has no white swell-flash on the
  model (the entity_model shader's hurt tint is red-only — the prime hiss + audio
  carry it; a swell flash would need a shader uniform — backlog).

WHAT THE USER SHOULD TEST (NO new world; re-run `import_mc_assets.py` first for
the creeper skin + sounds): press K to spawn a creeper ahead — it should chase
you, and when you let it get within ~3 blocks it hisses, swells, and after ~1.5 s
EXPLODES, leaving a crater and knocking/hurting you (stand back to watch one blow
without dying). LMB-kill a creeper before it blows → it drops gunpowder. Grab TNT
+ flint & steel from the creative palette: place a TNT block, RMB it with flint &
steel → it hops up, blinks, and after ~4 s makes a bigger crater. Hold flint &
steel and RMB a creeper → it force-ignites (swells even if you back away). Confirm
obsidian/bedrock survive a blast (place obsidian from the palette next to TNT).
Craft TNT (gunpowder + sand in the checkerboard) and flint & steel (iron ingot +
coal) at a table. Confirm armor reduces the blast damage, and that drops from
carved blocks pop out + can be picked up. Quit + re-enter → creepers you left are
restored (their fuse resets); primed TNT is gone (expected).

## M36 — projectile system → Skeleton (+ player bow/arrows) (DONE & USER-VERIFIED)

DONE & USER-VERIFIED 2026-06-18. Roadmap step 4: build the
shared PROJECTILE system once (an arrow entity), then drive it from a Skeleton's
ranged bow AI and the player's bow — the foundation later reused for
snowballs/eggs/fireballs/thrown potions. Decisions confirmed with the user before
coding: content = creative palette + skeleton drops ONLY (no crafting recipes —
vanilla bow needs string, arrows need flint, neither exists yet, same gap M35
hit); FULL visual polish (first-person bow pull-back frames + the skeleton's
bow-aim pose). Mechanics ported from 1.12 (`EntityArrow`, `EntitySkeleton` /
`EntityAIAttackRangedBow`, `RenderArrow`, `ModelSkeleton`, `ItemBow`). NO new
world. RE-IMPORT REQUIRED on this machine (see the top re-import note: 6 atlas
tiles 108–113, the skeleton skin, the arrow texture, `mob/skeleton/` +
`random/bow.ogg`).

- THE SHARED ROUTINE (`EntityManager::Arrow` + `SpawnArrow`/`TickArrows`, split
  into game/src/entity/Projectile.cpp like Explosion.cpp): a vanilla EntityArrow
  port — `Arrow : Body` carries flight yaw/pitch (+prev for interpolation), an
  `ArrowOwner {Player, Mob}`, base damage, a despawn `life` (1200 ticks), a
  `stuck` flag, and `playerPickup`. Per tick: gravity 0.05 b/tick² (20 b/s²) +
  drag ×0.99 (×0.6 in liquid); the segment travelled this tick is swept-tested —
  ENTITY first (Player arrows → `RaycastMob` → `DamageMob`; Mob arrows →
  segment-vs-player-AABB slab test → the injected `m_damagePlayer`), then BLOCK
  (`World::RaycastBlocks`; on hit it snaps to the surface + sticks). Impact damage
  = vanilla `ceil(speedPerTick × baseDamage)`. NOT persisted (vanishes on quit,
  like PrimedTnt / dropped items). Ticked from `EntityManager::Tick()` where
  m_mobs is idle, so a Player arrow's DamageMob (which erases the mob) is safe.
- PLAYER-TARGET PLUMBING: the M35 `SetExplosionTargets` was renamed/extended to
  `SetEntityTargets(playerFeet, halfWidth, height, damagePlayer)` so mob arrows
  can hit the player while World stays Player-agnostic; GameApp injects it before
  `World::Tick()`. Arrow pickup: `PickupArrows(box, give)` mirrors PickupItems —
  GameApp collects stuck player-fired arrows into the bag next to its item vacuum.
- ARROW RENDER (`entity/ArrowModel.{h,cpp}`): a verbatim RenderArrow port — a
  cross-prism shaft + a fletching cross, skinned from the 32×32
  `entity/projectiles/arrow.png`, drawn with the entity_model shader in its own
  pass (after the mob pass) oriented by the interpolated yaw/pitch (rotate yaw−90
  about Y, pitch about Z, the 45° cross offset about X, scale 0.05625). Skips
  without the skin (clean-clone rule).
- SKELETON (a data row + a ranged AI branch + a model): `MobType::Skeleton` (=6)
  appended; `MobDef` grew `ranged` + `bowRange` (15) + `shootInterval` (30).
  Row: 0.3×1.99, 20 hp, ~3.4 b/s, hostile, attackDamage 0 (never bites),
  followRange 16, SpawnRule::Dark weight 100 (shares the night roster with
  zombie/creeper). Runtime-only `Mob` fields `aiming` + `shootCooldown` (not
  persisted). TickMobs ranged branch: face the player, approach when beyond
  bowRange, only BACKPEDAL when very close (< 0.3×bowRange ≈ 4.5 blocks) and at a
  reduced 0.6× speed (so the player's 4.3 walk runs down the 3.4 skeleton — the
  earlier 0.5×-range full-speed retreat made it uncatchable), hold otherwise; on
  a clear line of sight (a
  RaycastBlocks eye→player check) fire every shootInterval ticks via
  `ShootArrowAt` (vanilla arc dy + horiz·0.2, speed 1.6 b/tick, slight
  inaccuracy, ArrowOwner::Mob, + a `MobSound` kind 4 = bow shoot). Drops arrows
  0–2 + bones 0–2.
- SKELETON MODEL: reuses `HumanoidModel` with the skeleton skin, the new
  `thinArms` (2px limbs) and `Pose::BowAim`. The `bool zombiePose` ctor arg
  became a `Pose {Default, Zombie, BowAim}` enum (zombie ctor + PlayerDoll
  updated). `SetVariant` toggles the aim pose (both arms raised straight ahead,
  vanilla ModelBiped bow-aim), driven each frame from the mob's `aiming` flag in
  the render pass. CRITICAL SKIN GOTCHA: the 1.12 skeleton skin is 64x32 (vanilla
  `ModelSkeleton` calls `super(.., 64, 32)`), NOT 64x64 like the zombie — the
  `HumanoidModel` is constructed with `texH=32` or every UV island samples the
  wrong rows and the whole figure renders as a featureless grey blob (the bug the
  user caught first). The HELD BOW is rendered by `HeldBowModel`
  (entity/HeldBowModel.{h,cpp}) — a flat two-sided quad skinned from a raw-copied
  `items/bow_standby.png`, drawn with the entity_model shader at
  `HumanoidModel::RightArmTransform` (which exposes the arm joint's world frame
  via `BoxModel::PartTransform`, refactored out of Render). The mob pass reads
  ONLY the hand WORLD POSITION from that transform (arm-local (0,9,0)), then
  builds the bow's orientation as a world-space basis (reasoning it through the
  Y-down + upright-flip + arm-rotation chain produced an oversized, skewed bow):
  the bow sits in the skeleton's SAGITTAL plane (up × aim), flat face sideways,
  so it reads edge-on when aiming at the player and shows the full profile from
  the side (vanilla). The `bow_standby` sprite runs grip→tip on its diagonal, so
  the basis is rolled 45° (`bx=(fwd+up)·k`, `by=(up−fwd)·k`, normal=`right`) to
  stand grip→tip vertical with the curve depth along the aim. `bs` (0.7/16 ≈ 0.7
  block) sizes it; user-verified 2026-06-18.
- PLAYER BOW (items + RMB-hold draw): `items::Bow` (108, 384-use, maxStack 1),
  `Arrow` (112), `Bone` (113); `kBowPullingTiles[3] = {109,110,111}` are the
  draw-frame tiles (view-model only, not items). All auto-listed in the creative
  palette. RMB-hold plumbing (the new piece — everything else is press-edge):
  GameApp::HandleInput intercepts RMB ahead of the place chain when the held item
  is a bow — `m_bowDrawSeconds` accumulates while held + an Arrow is in the bag
  (free in fly/creative); on RELEASE, `ReleaseBow` fires if the draw ≥ vanilla
  0.1. `BowDrawProgress()` = vanilla `(f²+2f)/3` clamp 1 (full at ~1 s); velocity
  = f·3.0 b/tick, damage 2.0 (2.5 at full = crit feel), consumes one Arrow
  (skipped in fly), wears the bow one use, plays `random/bow`. The view model
  steadies (no swing) and swaps the held bow to bow_pulling_0..2 by charge with a
  small pull-back nudge.
- AUDIO: `GameSounds` per-MobType voice array grew to 7 (skeleton say/hurt/death
  from `mob/skeleton/`); `PlayBowShoot` (random/bow) added; GameApp maps MobSound
  kind 4 → PlayBowShoot, and ReleaseBow plays it for the player.
- TESTABILITY: debug key `J` (KeyCodes grew `Key::J = 74`) spawns a skeleton ~3
  blocks ahead. Bow + arrows + bone are all in the creative palette. savetest grew
  a skeleton (type 6) mobs.dat round-trip case.
- ARROW KNOCKBACK: the player-damage callback gained a `knockbackScale`
  (Player::Hurt's 3rd arg) — mob melee + explosions pass 1.0, arrows pass 0.25
  (vanilla un-enchanted arrows barely knock back; full melee knockback made a
  kiting skeleton impossible to chase). Threaded through MobTickCtx.damagePlayer
  + EntityManager's injected callback.
- KNOWN M36 LIMITS / decisions: no crafting recipes (palette + drops only, by
  decision); the skeleton's held-bow hand placement is an eyeballed best-effort
  (tweak the GameApp constants if it sits oddly); arrows aren't persisted
  (in-flight + stuck arrows vanish on quit, like TNT);
  the arrow's block-stick uses RaycastBlocks (no sub-cell offset poke-out, no
  pop-out when the host block is broken); no critical-hit particles, no flaming/
  enchanted arrows, no tipped/spectral arrows; mob arrows can't hit other mobs
  (only the player) and player arrows only hit mobs (no friendly-fire either way);
  blast/arrow share the cheap single-ray LOS like M35.

WHAT THE USER SHOULD TEST (NO new world; re-run `import_mc_assets.py` first for
the skeleton skin, arrow texture, and sounds): press `J` for a skeleton — it
should turn toward you, keep its distance (back off if you close in), raise its
arms in the bow-aim pose, and fire arrows that ARC, STICK in blocks/ground, and
HURT you with knockback (armor should reduce it). Grab a Bow + Arrows from the
creative palette (E → palette); RMB-HOLD to draw — the first-person bow should
pull back through its frames — and RELEASE to fire: the arrow flies, one arrow is
spent, the bow wears a use; a fuller charge flies faster + hits harder. Walk over
your own stuck arrows to pick them back up. LMB-kill a skeleton → it drops arrows
+ bones you can collect. Let night fall (T to fast-forward) → skeletons spawn
alongside zombies/creepers. Quit + re-enter → skeletons you left are restored from
mobs.dat; in-flight/stuck arrows are gone (expected). The skeleton should hold a
visible bow in its hand — if it sits oddly (wrong angle/offset), say so and the
GameApp hand-placement constants can be nudged.

## M34 — passive mob roster: cow + sheep + chicken (CODE COMPLETE)

CODE COMPLETE 2026-06-18, awaiting user verification. The first step of the
post-M32 "Mob & enemy roadmap" (step 2: passive roster). Builds entirely on the
M32 mob framework — three passive animals, plus the TWO small framework
generalizations the roadmap wanted slipped in here. Decisions confirmed with the
user before coding: include ALL the signature behaviors (chicken slow-fall, sheep
wool drop, sheep shearing, chicken egg-laying); sheep are WHITE ONLY (16-colour
wool + dyeing is a later farming milestone). NO new world needed (gameplay only).
RE-IMPORT REQUIRED on this machine: `python scripts/import_mc_assets.py` — M34
added 8 atlas tiles (white wool 95 + beef/leather/mutton/chicken/feather/egg/
shears 96..102), the cow/sheep(+fur)/chicken skins, and three mob sound families
(189 sound files now). A clean clone with no overlay draws no cow/sheep/chicken
and is silent for them, exactly like the M32 pig/zombie.

- TWO FRAMEWORK GENERALIZATIONS (the roadmap's "slip these in during step 2"):
  - `World::SpawnMobs` (entity/Mob.cpp) is now DATA-DRIVEN. `MobDef` grew
    `SpawnRule` (SurfaceDay vs Dark) + `spawnWeight`; SpawnMobs decides which rule
    a candidate spot satisfies (grass + sky-light≥9 + day, or block-light≤7 +
    dark) then WEIGHTED-picks among the mobs sharing that rule (vanilla biome
    SpawnListEntry weights: sheep 12, pig 10, chicken 10, cow 8; zombie owns Dark).
    A new mob is now a table row, not an if-branch.
  - The render path carries a per-mob `MobDef::modelScale` folded into the
    upright-flip matrix around the feet (so a scaled mob still stands on the
    ground). All 1.0 today — it's the hook baby animals / size-varied slimes will
    multiply.
- MODELS (`game/src/entity/`): a shared render contract `vc::IMobModel`
  (MobModel.h: Ready / SetVariant / SetRotationAngles / Render) that every mob
  model implements, so GameApp's mob pass + sounds drive any mob uniformly via a
  `std::array<unique_ptr<IMobModel>, MobType::Count>` indexed by type (replaces
  the old hardcoded pig-vs-zombie if/else). HumanoidModel + PigModel retrofitted
  onto it. NEW models, all ported verbatim from the 1.12 model classes (looked up
  in `D:\Minecraft source code`): `CowModel` (ModelCow — quadruped + horns +
  udder, 64x32), `ChickenModel` (ModelChicken — head/bill/chin/body/2 legs/2
  wings; legs trot, wings sine-flap off `age`), and `SheepModel` — TWO stacked
  quadruped layers mirroring vanilla RenderSheep: the visible BODY is ModelSheep2
  (sheep.png, full-height legs) and an inflated WOOL layer is ModelSheep1
  (sheep_fur.png) drawn over it only when not sheared. `SetVariant(1)` hides the
  wool (a sheared sheep). White wool is rendered untinted (colour/dyeing is later).
  All five mobs' feet sit at model-y 24, so `modelOffsetPx` is 24 for every mob.
- CONTENT (data, not new code paths): white wool BLOCK (Block.cpp, tile 95, cloth
  material, drops self) + 7 items (Item.cpp, tiles 96..102): raw beef, leather
  (`items::LeatherItem` — the bare name collides with the `ArmorMaterial::Leather`
  enum), raw mutton, raw chicken, feather, egg, and shears (238-use damageable
  tool, no dig bonus). Both atlas scripts grew the same 8 tiles in the same order
  (gen_textures.py placeholders + import_mc_assets.py real art); texture array is
  103 layers now.
- DROPS: `MobDrops(type, sheared)` (entity/Mob.cpp) replaces the single-item
  `MobDropItem` — returns a list of {item, min, max}: pig porkchop 1-3, zombie
  rotten flesh 0-2, cow beef 1-3 + leather 0-2, sheep mutton 1-2 + (if not sheared)
  1 white wool, chicken raw chicken 1 + feather 0-2. EmitMobDeath rolls each.
- SPECIES BEHAVIOR (entity/Mob.cpp TickMobs):
  - Chicken slow-fall: `MobDef::slowFall` damps descent to 60%/tick while airborne
    (vanilla EntityChicken) and SKIPS fall damage.
  - Chicken egg-laying: `MobDef::laysEggs` + a per-mob `eggTimer` (6000..11999
    ticks = vanilla 5–10 min, runtime-only, restarts on load); on elapse it drops
    an egg item at the feet + queues an egg "plop" sound (MobSound kind 2). NOTE:
    real-tick timer (T fast-forward is world-time, not mob ticks), so don't wait
    on it during a quick test.
  - Sheep shearing: `EntityManager::ShearMob` sets the `sheared` flag (runtime
    only — a reloaded sheep grows wool back) and RETURNS the wool count (1-3).
    Player path: `GameApp::TryShearSheep` (RMB with shears in hand, mob nearer
    than any block target → shear, ADD the wool straight to the inventory (toss
    overflow), wear the shears one use, play the snip; mirrors TryUseBucket and
    sits first in the RMB chain). Wool goes directly to the bag — unlike a
    sheep-KILL (death drop scatters wool as a world item like all loot) — because
    it's an active tool harvest the player triggered (the first cut scattered it
    on the ground where it was easy to miss; the user asked for it in the bag).
- AUDIO: GameSounds generalized from the M32 pig/zombie pair to a per-`MobType`
  voice array (`MobVoice{say,hurt,death}` indexed by type; folder from
  `MobSoundFolder`). Cow/sheep/chicken have no death/hurt clips in 1.12, so
  PlayMobHurt/Death fall back to "say" (the existing pig pattern). Sheep `shear`
  and chicken `plop` are UNNUMBERED files in 1.12 (the numbered LoadSet probe
  misses them), so they're loaded by explicit name like splash. `PlaySheepShear` /
  `PlayChickenEgg` added.
- TESTABILITY: debug spawn keys — V = cow, N = sheep, M = chicken (alongside M32's
  B = pig, C = zombie; new key codes added to engine KeyCodes.h). A NEW world's
  legacy kit is unchanged; armor/etc come from the creative palette as before.
- PERSISTENCE: unchanged mobs.dat format (type/pos/yaw/health) — new MobTypes are
  just higher ints; sheared/eggTimer are runtime-only by design. savetest grew a
  sheep (type 3) round-trip case.
- CREATIVE PALETTE SCROLL (post-M34, user asked — the growing item count was
  overflowing the screen): the palette is now a fixed-height WINDOW
  (`kPaletteVisibleRows` = 6) that scrolls with the mouse wheel + a scrollbar
  thumb on the right edge, instead of one grid that grew off-screen. `GameApp`
  owns the offset (`m_paletteScroll`, rows; advanced from the wheel only while
  `State::Inventory`), `InventoryScreen::Draw` clamps it + renders only the
  visible window. Vanilla-style CATEGORY TABS (Blocks/Tools/Combat/Food/Misc)
  are the agreed future step once the count justifies categories — the scroll
  work is the foundation they'd build on (backlog).
- KNOWN M34 LIMITS / decisions: sheep are white only (no colour/dye, no
  grass-eating regrow — a sheared sheep stays sheared until you reload); chicken
  egg cycle + sheep sheared state aren't persisted (restart resets them); chicken
  wings do a constant gentle idle flap (no faster falling-flap — would need the
  vanilla wingRotation sim); no cow milking (needs a bucket-on-mob interaction);
  no baby animals / breeding (that's the food/eating milestone, roadmap step 5);
  passive mobs share one cap of 8 within 64 blocks.

WHAT THE USER SHOULD TEST (NO new world; re-run `import_mc_assets.py` first for
the skins + sounds): press V / N / M to spawn a cow / sheep / chicken ahead of
you — each should stand on the ground and amble in random directions with legs
trotting (chicken wings flap, sheep is fluffy). LMB-kill one → it flashes red and
drops its loot (cow: beef + maybe leather; sheep: mutton + 1 wool; chicken:
chicken + maybe feathers) that you can pick up; killing a pig still drops
porkchop. Grab SHEARS from the creative palette, hold them, RMB a sheep → snip
sound, the sheep visibly loses its wool (gets thinner) and drops white wool;
RMB it again → nothing (already sheared). Drop a chicken off a cliff → it flutters
down slowly and takes NO fall damage (other mobs still take fall damage). Let the
clock run / wander around → cow/sheep/chicken/pig should appear naturally on grass
in daylight (mixed species), zombies still at night. Quit + re-enter → the animals
you left are restored (sheared sheep come back woolly — expected). Egg-laying is on
a 5–10 minute timer (you'll hear a "plop" and see an egg pop out) — not something
to wait on during a quick pass.

## Planned arc — survival & mobs (M30–M33)

Scoped with the user 2026-06-15. Goal: enemies, a health system, armor, and a
player character shown in the inventory UI. These build on each other, so they
land IN ORDER, one milestone at a time, each in its own context where possible.
The KEY insight that drives the ordering: the humanoid BOX-MODEL RENDERER is a
shared dependency of both "enemies" and "show the player in the UI" — health and
armor LOGIC are cheap, but the jointed-model RENDERING is the real new engine
capability. So health (HUD-only, no new renderer) goes first, the model renderer
goes second, and mobs + armor + the player doll fall out of it.

What we already have working in our favor (don't rebuild these):
- An ENTITY pattern: `World::FallingBlock` + `World::ItemEntity` (World.h ~141)
  are tick-simulated, prev/current interpolated, AABB-collided structs. A mob is
  the same shape + health + AI + a model. Generalize, don't invent.
- ARMOR UI SLOTS already exist as inert art in the inventory panel (M17 note:
  "Armor/crafting regions are inert art until M19" — crafting got wired, armor
  didn't). The slots draw; they just do nothing yet.
- 3D-MODEL-INTO-UI is solved: M29's `vox::Framebuffer` + `BlockIcons` bake a 3D
  model to an offscreen sheet and blit it through the 2D UI. The player doll is
  the same technique with a humanoid model instead of a cube.
- Mature item/tool/durability systems (M19), damage SOURCES already present but
  inert (lava/cactus/fall "no damage — no health system" notes throughout).

- **M30 — Survival IV: health, damage & hunger** ✅ (DONE & USER-VERIFIED — see
  the detailed "M30 — health, damage & hunger" section above). Player
  `health` (20 = 10 hearts) + `hunger`/saturation (20 = 10 drumsticks) on
  `Player`, ticked at 20 TPS. Damage sources wired into the existing tick — the
  hooks are already stubbed everywhere ("no damage" notes): fall damage (vanilla
  `max(0, fallDistance-3)` half-hearts), lava/fire (per-tick + burn timer),
  cactus touch, drowning (air supply once submerged past the head), void (y<0).
  Natural regen gated on hunger ≥ 18 (vanilla 1.12 rules); starvation damage at
  hunger 0; hunger drains via exhaustion (move/jump/dig). Death → a respawn
  screen → respawn at spawn with full health, drops handled later. HUD: heart row
  + hunger row + (when submerged) air bubbles, drawn through the existing
  icon-drawing path (icons.png has the vanilla heart/food/bubble sprites). No new
  renderer, no mobs — fully self-contained. Persist health/hunger in level.dat.
  This is the milestone we START now; the rest go in fresh contexts.

- **M31 — Entity model renderer (engine)** ✅ DONE & USER-VERIFIED (see the
  detailed "M31 — entity model renderer" section above).
  THE shared bottleneck. A jointed
  multi-cuboid "box model" renderer (vanilla `ModelBase`/`ModelRenderer`): named
  parts with per-part pivot + rotation, a single skin texture with per-box UV
  layout, hierarchical transforms, render-interpolated. Lives in the engine
  (reusable), generalizes the FallingBlock/ItemEntity cube path. Ships with one
  test model (the Steve humanoid: head/body/2 arms/2 legs) and an idle/walk
  animation, drawn on a debug entity — NO gameplay yet. Both mobs and the player
  doll consume this.

- **M32 — Mobs (AI, spawning, combat)** ✅ CODE COMPLETE (see the detailed
  "M32 — mobs" section above). A `Mob` entity over M30/M31: pig + zombie,
  wander/idle vs chase-and-melee AI, light/cap spawn rules + despawn, two-way
  melee with knockback + red hurt-flash, item drops, mobs.dat persistence.

- **M33 — Armor & the player doll in the inventory UI** 📋. Equippable armor
  items (helmet/chest/legs/boots × a material tier) with durability (M19) and
  damage reduction applied in M30's damage path; activate the inert armor slots
  (equip on click / shift-click). Render the PLAYER CHARACTER in the inventory
  screen — the M31 Steve model baked to the M29 framebuffer→UI path — showing the
  worn armor layer. Once M31 exists this is mostly wiring + the armor-layer skin.

## Mob & enemy roadmap (post-M32, scoped with the user 2026-06-16)

M32 shipped the mob FRAMEWORK (per-type AABB + health, wander/chase/melee AI,
light/cap spawning, two-way knockback combat, drops, persistence, the biped +
quadruped box-model renderer, hurt flash, per-mob sounds). The rest of the
bestiary splits into "drops onto that framework for free" vs. "needs a shared
system built first." This is the agreed ORDER for future milestones; do them
roughly top-to-bottom, each its own milestone/context.

FRAMEWORK-READY (no new system — a `MobDef` row + a model + a skin + a drop):
- Cow / Sheep / Chicken / Mooshroom (passive). Cow/sheep reuse the quadruped
  `PigModel` shape with different box dims + skin (cow is the trivial one —
  beef/leather); chicken is its own small model. Sheep adds wool/shearing/color
  + grass-eating; chicken adds egg-laying + slow-fall — those are small extras.
- Husk / drowned-style zombie reskins.
- Spider works NOW as a plain wide melee mob (per-type AABB already handles its
  2-block width); wall-climbing is a later movement extension.

NEEDS A SYSTEM FIRST (each system also unlocks player features — build once,
reuse):
- Creeper → an EXPLOSION system (sphere block-removal + damage falloff + fuse/
  ignite). ✅ DONE in M35 (see the M35 section): the shared
  `EntityManager::Explode` also powers TNT now and is ready for ghast fireballs,
  beds, end crystals.
- Skeleton → a PROJECTILE/arrow entity + ranged AI. ✅ DONE in M36 (see the M36
  section): the shared `EntityManager::Arrow` also powers the player bow & arrows
  now and is ready for snowballs, eggs, fireballs, thrown potions.
- Breeding / baby animals → a FOOD/EATING system (+ a model SCALE factor for
  babies). Also makes the M32 porkchop/rotten-flesh drops actually useful;
  pairs with a farming milestone.
- Slime → split-on-death (spawn smaller children) + size variants + bounce.
- Enderman → teleport + block-carry + look-to-aggro (mostly bespoke).

RECOMMENDED ORDER:
1. **M33 — armor + inventory player doll** ✅ DONE.
2. **M34 — passive roster: cow + sheep + chicken** ✅ CODE COMPLETE (see the M34
   section above). Both small framework generalizations landed here: SpawnMobs is
   now data-driven (per-MobDef SpawnRule + weight) and the render path carries a
   per-mob modelScale (baby/variant hook).
3. **Explosion system → Creeper (+ TNT)** ✅ CODE COMPLETE (see the M35 section
   above). The shared `EntityManager::Explode` drives both the creeper and a
   primed-TNT entity; ready to reuse for ghast fireballs, beds, end crystals.
4. **Projectile system → Skeleton (+ player bow/arrows)** ✅ CODE COMPLETE (see
   the M36 section above). The shared `EntityManager::Arrow` drives both the
   skeleton's ranged bow AI and the player's bow; ready to reuse for
   snowballs/eggs/fireballs/thrown potions.
5. **Food/eating** (vanilla `ItemFood`) ✅ DONE + USER-VERIFIED (M37 section).
   **Breeding + baby animals** ✅ CODE COMPLETE (M38 section): feed two adults
   their breed item → love mode → a half-scale child (the M34 `modelScale` hook is
   the baby-size lever); baby grows up on a timer; baby state persists. Pairs with
   a future farming milestone (which would give the breed items a real source).
6. **Bespoke movers** as desired: spider climbing ✅ CODE COMPLETE (M39 — see the
   M39 section), then slime splitting / enderman teleport / bats as desired. ←
   spider was the first; the rest are roughly NEXT (or a FARMING milestone — crops
   would give wheat/carrot/seeds an in-world source beyond the creative palette).

TWO SMALL GENERALIZATIONS to slip in during step 2 — both ✅ DONE in M34 (see the
M34 section): `World::SpawnMobs` (now in game/src/entity/Mob.cpp) is data-driven
via `MobDef::spawnRule` + `spawnWeight`, and the mob render path folds in
`MobDef::modelScale` (the baby/slime size multiplier hook, all 1.0 today).

## M39 — Spider (bespoke movers I: wall-climb) (how it works)

DONE and USER-VERIFIED 2026-06-19 ("that works"). The FIRST roadmap-step-6
"bespoke mover": a wide melee crawler that CLIMBS walls and is NEUTRAL in
daylight. Builds entirely on the M32/M34 mob framework — no new system, the
bespoke parts are two data-driven `MobDef` flags + their handling in `TickMobs`.
RE-IMPORT on this machine: `import_mc_assets.py` (spider skin + `mob/spider/`
sounds + 2 atlas tiles → 123 layers).

- DATA (`Mob.h`): `MobType::Spider` appended (id 7; mobs.dat ids stay stable).
  `MobDef` grew two trailing bools WITH default initializers (`canClimb` /
  `neutralInLight`, both default false) so only the spider row lists them — the
  other 7 rows are untouched (aggregate init omits the defaulted trailing
  members). Spider row: 1.4×0.9 AABB (halfWidth 0.7, height 0.9 — wide + low,
  vanilla `setSize(1.4, 0.9)`), 16 hp, 4.1 b/s (vanilla 0.30 movementSpeed scaled
  off the pig's 0.25→3.4), hostile melee 2, `SpawnRule::Dark` weight 100 (even
  with zombie/creeper/skeleton at night), `canClimb`+`neutralInLight` true.
- CLIMB (`TickMobs`, vanilla `PathNavigateClimber` + `isOnLadder`): a runtime
  `Mob::besideClimbable` flag mirrors vanilla's `setBesideClimbableBlock(
  isCollidedHorizontally)`. It's REFRESHED at the end of the move (after the
  auto-step block) and READ at the start of the next tick's gravity phase — the
  vanilla ordering (set in onUpdate, used next moveEntityWithHeading). Refresh
  rule: `besideClimbable = moving && (hitX||hitZ) && movedSq < wantSq*0.25` — i.e.
  it WANTED to move but a horizontal collision kept it from advancing. That
  `movedSq < wantSq*0.25` test is what excludes a successful 1-block AUTO-STEP
  (which advances fully) from triggering a climb — climb only engages on walls
  taller than the step. When climbing, the gravity phase overrides `vel.y =
  kClimbSpeed` (4 b/s up, vanilla motionY 0.2 b/tick) and zeroes fallDistance, so
  it scales the wall; once it clears the top there's no more horizontal collision,
  besideClimbable goes false, and gravity drops it onto the ledge. All gated on
  `def.canClimb`, so every other mob is byte-for-byte unaffected.
- NEUTRAL-IN-LIGHT (`TickMobs`): the hostile-chase branch gained `&& (!def
  .neutralInLight || IsDarkAt(mob.pos, def.height))`. `IsDarkAt` (a lambda at the
  top of TickMobs) mirrors the `SpawnRule::Dark` gate (blockLight ≤ 7 AND (night
  OR skyLight ≤ 7)) at the mob's body cell. In daylight the condition fails → the
  spider falls through to the wander branch (vanilla `AISpiderAttack`/`AISpiderTarget`
  give up when brightness ≥ 0.5). It still climbs walls while wandering (vanilla —
  climbing is target-independent). NOTE: a daytime spider does NOT retaliate when
  hit (the framework has no hurt-by-target/aggro-on-attack concept yet) — minor
  deviation, deferred.
- MODEL (`entity/SpiderModel.h/.cpp`): a verbatim port of vanilla ModelSpider on
  `vox::BoxModel` (64×32 skin) — head (texOffset 32,4), neck (0,0), body (0,12),
  and 8 legs (all texOffset 18,0; right legs origin -15, left legs -1, pivots
  stepping z 2,2,1,1,0,0,-1,-1). `SetRotationAngles` ports the leg base Z/Y splay
  + the four-phase scuttle (fY cos at 2× freq, fZ |sin|), applied in the BoxModel's
  vanilla Z-then-Y-then-X order. Uses the standard `modelOffsetPx` 24 like every
  mob; the splayed legs rest ~1px into the ground (cosmetic, reads grounded —
  matches vanilla's low slung look). Wired in GameApp's mob-model table + the
  generic mob render pass handles it (no variant, baby/modelScale fold in as usual).
- DROPS / SOUNDS / ITEMS: `MobDrops` → string 0-2 + spider eye 0-1 (player-kill
  secondary, like vanilla). `MobSoundFolder` → "spider"; the per-type voice loop
  auto-loads `mob/spider/` (say/step/death — no distinct hurt, falls back to say;
  `kMobVoiceCount` bumped 7→8). New items `items::String` (121) + `items::SpiderEye`
  (122), plain sprites appended after M38's seeds in BOTH atlas scripts; spider eye
  is non-food for now (vanilla poison waits for a status-effect system — kept a
  sprite-only brewing ingredient like gunpowder). The creative palette auto-lists
  both. `import_mc_assets.py` got the spider skin COPY + the 2 atlas tiles +
  `mob/spider/` in `want_sound`.
- TESTABILITY: debug key `L` spawns a spider ~3 blocks ahead. Persistence is
  type-generic (mobs.dat), so a spider saves/loads like any mob — savetest needed
  no change and still passes (incl. the pre-M38 back-compat read).
- KNOWN M39 LIMITS / deferrals: natural spider spawns use the framework's 1-WIDE
  column clearance check, so a 1.4-wide spider can spawn snug against a wall (the
  first ticks' collision resolves it; debug spawns drop it in open air); no spider
  jockey (skeleton riders), no leap-at-target, no cave spider, no climbing-pose
  head/body tilt, no daytime retaliation. More step-6 movers (slime/enderman/bats)
  are the next candidates.

WHAT THE USER SHOULD TEST (NO new world; re-run `import_mc_assets.py` first for
the spider skin + sounds): press `L` → an eight-legged spider drops in and
scuttles (verify the body sits low on splayed, trotting legs + it makes spider
chittering). Build a wall ≥2 blocks tall in front of it and stand on top / behind
it AT NIGHT (or in a dark cave) → it should chase, hit the wall, and CLIMB
straight up to reach you (not just stand there). In DAYLIGHT on the surface a
spider should ignore you and wander (it may still climb walls it bumps). Hit it
with a sword → string (and sometimes a spider eye) pop out and vacuum up; both
also appear in the creative palette. Quit + re-enter → a spider you left is still
there. Spiders should also appear naturally at night / in dark caves alongside
the other hostiles.

## M38 — Breeding + baby animals (how it works)

DONE and USER-VERIFIED 2026-06-18 ("that works"). Roadmap step 5's PAYOFF —
feed two adult animals → love mode → a scaled-down child; the M34 `modelScale`
hook is the baby-size lever. Decisions confirmed with the user before coding:
vanilla per-species breed items (NOT a universal item), uniform half-scale babies
(NOT vanilla big-head proportions), and PERSIST baby + grow state across reload.
Built on the M37 food milestone + the M32/M34 mob framework. No new world needed.
RE-IMPORT on this machine: `import_mc_assets.py` (3 new atlas tiles → 121 layers).

- BREED ITEMS (`item/Item.h/.cpp`, sprite tiles 118 wheat / 119 carrot / 120
  seeds — appended after the M37 cooked foods; BOTH atlas scripts grew the same 3
  tiles, gen_textures.py placeholders + import_mc_assets.py real art:
  `wheat.png`/`carrot.png`/`seeds_wheat.png`). Plain NON-FOOD sprites for now
  (vanilla carrot edibility waits for farming; non-food also means RMB-feeding
  never collides with the M37 hold-to-eat gate — that gate only fires on
  `IsFood`). The creative palette auto-lists them. They have no in-world source
  yet (farming milestone) — grab them from the palette.
- WHICH ITEM BREEDS WHAT (`Mob.cpp` `IsBreedingFood(type, item)` — a runtime-id
  switch like `MobDrops`, NOT a `MobDef` field since item ids are runtime):
  wheat → cow + sheep, carrot → pig, seeds → chicken (vanilla 1.12). Hostiles +
  any non-breed item return false → RMB falls through.
- MOB STATE (`Mob.h`, runtime + persisted split): `baby` (renders at
  `kBabyModelScale` 0.5, can't breed/lay eggs), `growUpTimer` (ticks to
  adulthood, vanilla 24000 = 20 min), `loveTimer` (>0 = adult in love seeking a
  mate, vanilla 600 = 30 s), `breedCooldown` (post-mating lockout, vanilla 6000 =
  5 min). Tuning constants live in `Mob.h` (`kBabyGrowUpTicks` etc).
- FEED (`GameApp::TryFeedMob`, in the RMB place chain right after `TryIgnite`,
  mirrors `TryShearSheep`: gate on the held breed item → raycast a mob → reject if
  a block target is nearer): calls `EntityManager::FeedMob(index, item)`, which —
  for an ADULT not in love + off cooldown → sets `loveTimer` (love mode); for a
  BABY → speeds growth 10% (`growUpTimer -= kBabyGrowUpTicks/10`); else returns
  false (no consume, RMB falls through). On accept GameApp consumes one, swings,
  and plays the eat munch (`Sounds::PlayEat`) at the animal.
- MATING (`TickMobs`, a new AI branch between hostile-chase and wander): an adult
  with `loveTimer > 0` scans for the nearest same-species adult also in love +
  off cooldown within 8 blocks, walks to it (full chase speed), and when within
  1.6 blocks queues a baby at the midpoint, zeroes both `loveTimer`s, and arms
  both `breedCooldown`s. Babies are DEFERRED to after the loop (like creeper
  detonations) because `SpawnMob` push_backs into `m_mobs` and could realloc the
  `mob&`. Setting the partner's `loveTimer = 0` by index means the loop won't
  double-breed it later the same tick.
- BABY GROW-UP / RENDER: the per-mob timer decrements sit with the other tick
  decrements (so they run even when the AI freezes on an unloaded chunk); at 0 the
  baby flips to adult. The render path folds the baby 0.5 into the same
  feet-anchored scale as `MobDef::modelScale` (offset translate is applied AFTER
  the scale, so the smaller body still stands on the ground).
- PERSISTENCE (`WorldSave` `MobRecord` + mobs.dat): `baby` + `growUpTimer` are
  APPENDED to each mob line. `ReadMobs` now parses LINE-BY-LINE (getline +
  istringstream) so a pre-M38 6-field line still loads, defaulting to an adult —
  savetest covers both the baby round-trip AND the pre-M38 back-compat read.
  love/cooldown are runtime-only by design (a reloaded animal forgets it was
  courting), like the M34 sheared/egg state.
- TESTABILITY: debug key `H` spawns a baby cow ahead (verify the half-scale render
  + grow-up without breeding). Grab wheat/carrot/seeds (64-stacks) from the
  creative palette. Grow a baby fast by feeding it ~10 times (10%/feed) rather
  than waiting 20 min.
- KNOWN M38 LIMITS / deferrals: NO heart particles / XP orbs on breeding (the
  baby appearing + the eat sound are the feedback — hearts want a particle-sheet
  tile, backlog); babies keep the ADULT collision/raycast AABB (the model is half
  but the hitbox isn't — simpler, slightly generous to click); uniform half-scale,
  not vanilla's big-head baby proportions; one passive cap of 8 still applies, so
  a crowded pen won't breed past it.

WHAT THE USER SHOULD TEST (NO new world; re-run `import_mc_assets.py` first for
the breed-item sprites): press `H` → a tiny half-size baby cow drops in and ambles
(verify it stands on the ground, legs trot). From the creative palette grab WHEAT,
spawn two adult cows (`V` twice), hold wheat and RMB each cow → munch sound, item
count drops; the two cows walk toward each other and a BABY cow pops out between
them; feeding either again right away does nothing (5-min cooldown). RMB the baby
with wheat ~10 times → it grows to full size. Repeat the pairing with SEEDS on two
chickens (`M` spawns a chicken) and CARROT on two pigs (`B` spawns a pig). Feeding
the WRONG item (seeds to a cow) does nothing + isn't consumed. Quit + re-enter → a
baby you left is still a baby (and still aging). Eating still works normally for
real food (cooked meat) since breed items aren't food.

## M37 — Food / eating (how it works)

DONE and USER-VERIFIED 2026-06-18 (eating + furnace cooking both confirmed).
Wires vanilla `ItemFood` onto the existing meat drops + furnace cooking. This is
roadmap step 5's "food/eating" prerequisite; the breeding/baby payoff is the
next milestone.

- Data (`item/Item.h/.cpp`): `ItemDef` grew `food` + `foodPoints` +
  `saturationModifier` + `alwaysEdible`. Queries `IsFood`/`FoodPoints`/
  `FoodSaturation`/`AlwaysEdible` (FoodSaturation applies vanilla
  `foodPoints * modifier * 2`). Tagged with 1.12 `ItemFood` values: raw
  porkchop 3/0.3, raw beef 3/0.3, raw mutton 2/0.3, raw chicken 2/0.3, rotten
  flesh 4/0.1 + alwaysEdible. DEFERRED (need a status-effect system): rotten
  flesh's hunger/poison debuff, raw chicken's 30% food-poison chance.
- Cooking (`world/Furnace.cpp`): four new `SmeltResult` rows turn each raw meat
  into its cooked variant (cooked porkchop/steak/mutton/chicken, food values
  8/0.8, 8/0.8, 6/0.8, 6/0.6). The cooked items are new registry items (tiles
  114–117, appended after the M36 bone tile — added to BOTH `gen_textures.py`
  placeholders and `import_mc_assets.py` real sprites: `*_cooked.png`). The
  creative palette auto-lists them; any fuel cooks them like ore/sand.
- Player (`entity/Player.h/.cpp`): `CanEat(alwaysEdible)` = not dead AND
  (foodLevel<20 OR alwaysEdible); `Eat(points, saturation)` is vanilla
  `FoodStats.addStats` — hunger up to 20, then saturation tops up clamped to the
  new food level. Feeds the M30 fields directly; natural regen already runs off
  them.
- Input (`GameApp::HandleInput`): an eat gate right after the M36 bow gate. While
  RMB is held on a food the player can eat (walk-only — fly is creative),
  `m_eatSeconds` accumulates; at `kEatSeconds` (1.6 s = 32 ticks) `EatHeldFood`
  consumes one, calls `Player::Eat`, and burps. Owns RMB (early `return`) ONLY
  while actually eating, so right-clicking a block with food at full hunger still
  uses the block (CanEat gates out → falls through to the place chain). Every
  `kEatChewInterval` (0.30 s) `EmitEatChew` fires a crunch + crumb burst.
  `EatProgress()` (0..1) feeds the view model via the shared `SetUseProgress`
  (max of bow draw + eat — only one is ever non-zero).
- Feedback: `Sounds::PlayEat` (`random/eat1..3`, per bite) + `PlayBurp`
  (`random/burp`, last bite); `Particles::SpawnEatCrumbs(world, mouth, tile)` — a
  5-crumb downward burst textured from the food's SPRITE tile (an atlas layer,
  not a BlockId, so it bypasses the block-faced `Spawn`). The view model adds the
  vanilla EAT transform (chewing bob + swing-to-mouth) keyed on
  `IsFood(displayId)`. New sounds added to `import_mc_assets.py want_sound`
  (`random/eat1..3`, `random/burp`) — already imported here.

What the user should test: hold a raw meat / rotten flesh on the hotbar, take
some damage or sprint to drop hunger below full, then RMB-HOLD → the food shakes
up to the mouth, you hear chewing + see crumbs, ~1.6 s later a burp and the
hunger bar refills + one item is consumed; releasing early cancels with no
consume; at FULL hunger RMB does nothing for normal meat but rotten flesh still
eats (alwaysEdible); right-clicking a crafting table with food in hand at full
hunger still opens it; eating doesn't work in fly mode. Quit + re-enter → the
restored hunger/food persists (M30 vitals save). COOKING: put raw meat in a
furnace input slot + any fuel → after ~10 s out comes the cooked variant; the
cooked meats restore more hunger than raw and appear in the creative palette.

## Redstone roadmap (scoped with the user 2026-06-19)

The NEXT major arc (after parking the remaining bespoke movers). Redstone is the
hardest vanilla system to get right — but the fear is front-loaded into ONE
milestone (RS1, the power-propagation engine), and that engine is structurally
the `LightEngine` we already shipped and trust. Everything after RS1 is the same
"add data + a little behavior to an existing bus" pattern as adding a block or a
mob. Pistons (the other genuinely hard piece, because block MOVEMENT is its own
system) are cleanly separated into the last sub-arc.

WHY IT'S LESS SCARY THAN IT LOOKS — every prerequisite is already in the codebase:
- **Propagation = light.** Redstone power is a 0–15 value that radiates from
  sources and drops 1 per block — IDENTICAL in shape to block light. `LightEngine`
  already does BFS-from-sources / attenuate-per-step / COW snapshot / versioned
  recompute. A `RedstoneEngine` is that algorithm again, with directional/stateful
  twists layered on for repeaters & comparators (RS2/RS3).
- **Update model = M13.** `World::ScheduleBlockUpdate(pos, delayTicks)` +
  `ProcessBlockUpdate` already drive water/sand cascades at 20 TPS with arbitrary
  tick delays — exactly what redstone signal updates + repeater delays (2/4/6/8
  game ticks) need. No new tick infrastructure.
- **Component state = M24 meta.** The per-cell `GetMeta/SetMeta` byte already
  stores facing (4 bits); the spare bits hold lever on/off, repeater delay step,
  comparator mode. Block entities + persistence exist for anything bigger.
- **Component models = M23.** The torch is already a float-model block; lever /
  repeater / comparator / button reuse that stream. The lamp reuses the furnace
  front's lit/unlit texture-swap. Redstone ore drops into the M21 ore framework +
  M25 deep world.

THE ARC (each milestone is a runnable vertical slice; do them in order):

- **RS1 — power core + the minimal loop (lever → dust → lamp).** THE scary one.
  Build the `RedstoneEngine` (per-cell power 0–15, recomputed on M13 block updates
  over the affected network/region; model it on `LightEngine`). Add: redstone ore
  (deep underground, M21 framework, drops redstone dust), the redstone dust ITEM,
  the redstone WIRE block (flat overlay on a solid block's top face, tinted dark→
  bright red by power; v1 can connect to all neighbours and refine connection
  geometry later), the LEVER (toggle source, M23 model + M24 facing) and REDSTONE
  BLOCK (constant 15), and the REDSTONE LAMP (lit/unlit texture-swap consumer).
  SIMPLIFY for v1: skip the strong/weak-power distinction — a lamp lights if an
  adjacent wire has power>0 or an adjacent source is on; refine when RS2 needs it.
  Verify: lever → dust run → lamp lights when flipped. Once this works, redstone
  is "real" and the rest is incremental.
- **RS2 — logic + timing.** Redstone TORCH (a source AND an inverter — off when its
  attachment block is powered; introduces the NOT gate + torch burnout). REPEATER
  (one-way diode, restores signal to 15, 2/4/6/8-tick delay via the M13 scheduler;
  the trickiest timing piece — delay state in M24 meta). BUTTON (momentary pulse) +
  PRESSURE PLATE (powers while a player/mob stands on it — reuse the entity-position
  queries the mob/pickup code already does). Add the strong/weak block-powering
  rules deferred from RS1. Deliverable: real logic gates (AND/OR/NOT/XOR) buildable.
- **RS3 — analog + more consumers.** COMPARATOR (compare/subtract modes; reads
  container fullness → analog signal — needs a container fill-level query).
  DAYLIGHT SENSOR (reads sky light, which the day/night + light system already
  exposes). NOTE BLOCK. DISPENSER/DROPPER (spawn items — reuse the M18 item-entity
  / M36 projectile spawn path). Powered DOORS / TRAPDOORS / FENCE GATES.
- **RS4 — pistons (its own sub-arc — block movement).** PISTON + STICKY PISTON:
  push up to 12 blocks, the moving-block animation (reuse the M13 FallingBlock
  entity↔mesh handover pattern — hide the source cell, animate the entity, settle
  into the destination cell), immovable-block rules (obsidian/bedrock/block
  entities), sticky pull-back, drop-on-illegal-push. Biggest single piece; depends
  only on RS1's power core, so it can land well after RS2/RS3.
- **Deferred / possible RS5:** rails + minecarts (needs a rideable entity — a new
  system), hoppers (item transport), observers, and the BUD/quasi-connectivity
  quirks (probably skip — they're bug-features).

OPEN QUESTIONS to settle when RS1 starts: (a) does redstone go BEFORE or AFTER a
farming milestone (farming gives M38's wheat/carrot/seeds a real source; redstone
is the bigger lift) — the user leaned redstone-is-more-important; (b) wire
connection-geometry fidelity for RS1 (simple all-neighbours cross vs full vanilla
dot/line/L/T/up-the-side shapes — recommend simple first, refine later).

## Backlog (after M28)

With the M24 meta layer + M23 model stream + M28's partial collision all in
place, the natural follow-ons are STAIR AUTO-CORNERS (inner/outer shapes when
stairs meet — vanilla `BlockStairs.EnumShape`, the piece M28 deferred) and a
SETTINGS SCREEN (audio buses + `AudioEngine::SetBusVolume` already exist and
want a UI; also occlusion / render-distance / sensitivity). Other open
candidates, to scope with the user:
full 256-tall world (would want
empty-air-chunk culling first); a settings screen (the audio
buses + `AudioEngine::SetBusVolume` already exist and want a UI; also
occlusion / render-distance / sensitivity); tall-grass wheat seeds
(vanilla 1/8, BlockTallGrass.getItemDropped — pairs with farming);
flow-animated water (16x512 strip); stars; world-list scrolling;
vanilla's 14/16 cactus inset +
touch damage; 3D-extruded item sprites in hand + view bobbing (M20
polish). M32 deferrals: zombie daylight burning (sky-exposure check +
per-mob fire timer + flame visuals on the model); hard player↔mob
collision (M32 uses a soft mob-side push, so you can shove through a
mob); smarter mob pathing (A*/jump-over-gap instead of straight-line +
auto-step); more mobs (bespoke movers — spider climb / slime split / enderman
teleport — roadmap step 6). M37/M38 follow-ups: farmed foods +
crops (apple/bread/wheat/carrot/seeds growth — a FARMING milestone would give
the M38 breed items a real in-world source instead of the creative palette);
the rotten-flesh hunger / raw-chicken food-poison debuffs once a status-effect
system exists; breeding hearts + XP orbs (need a particle-sheet tile);
smaller baby hitboxes + vanilla big-head baby proportions.

## How to verify (working agreement — reinforced 2026-06-13)

Do NOT launch the game, take screenshots, or inject input from the agent
AT ALL. The user runs the game themselves and reports back. The agent's
job is: build (to confirm it compiles) + run the headless tests
(gentest/savetest), then hand the user a precise test checklist. (The user
is often in calls/recordings — popping windows + capturing the cursor is
disruptive, and they asked twice now to keep verification on their side.)

## Working agreements (see memory too)

- Milestone at a time; verify with a build + headless tests, then a user
  test pass before claiming done; tick the milestone in ARCHITECTURE.md.
- Keep this file updated at each milestone boundary or when context runs low.
