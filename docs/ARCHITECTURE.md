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
- **M5 — Meshing quality**: greedy meshing, ambient occlusion in vertex data.
- **M6 — Player**: AABB physics, collision, raycast block break/place.
- **M7 — Lighting**: BFS flood-fill block light + sky light, per-vertex light
  in mesh data.
- **M8 — Persistence**: region-file world saves.
- **M9 — Scale**: occlusion culling, multi-draw indirect, LOD for far chunks.

Each milestone lands as a vertical slice that runs; no big-bang integration.

## Conventions

- C++23, MSVC `/W4 /permissive-`. Format with the repo `.clang-format`.
- Engine code lives in `namespace vox`, files under `engine/src/vox/<module>/`.
- Includes are project-root-relative: `#include "vox/core/Log.h"`.
- `PascalCase` types/methods, `m_camelCase` members, `s_camelCase` statics.
- Engine logs via `VOX_*` macros, game logs via `GAME_*`.
