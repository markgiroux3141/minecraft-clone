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
- **M13 — TBD**: candidates: block updates (falling sand + flowing water
  — needs per-block metadata and scheduled ticks), visual polish (cutout
  leaves, water skylight attenuation, lowered water surface, moon/stars),
  caves/biomes, inventory UI.

Each milestone lands as a vertical slice that runs; no big-bang integration.

## Conventions

- C++23, MSVC `/W4 /permissive-`. Format with the repo `.clang-format`.
- Engine code lives in `namespace vox`, files under `engine/src/vox/<module>/`.
- Includes are project-root-relative: `#include "vox/core/Log.h"`.
- `PascalCase` types/methods, `m_camelCase` members, `s_camelCase` statics.
- Engine logs via `VOX_*` macros, game logs via `GAME_*`.
