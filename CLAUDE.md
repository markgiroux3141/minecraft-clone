# Voxcraft — Minecraft clone (C++23, OpenGL 4.6)

## Build & run

```powershell
.\scripts\build.ps1                 # debug build (use this to verify changes compile)
.\scripts\build.ps1 -Run            # build + launch the game
.\scripts\build.ps1 -Config release
```

Plain `cmake` won't work from a normal shell — MSVC needs the VS dev
environment, which the script sets up via vswhere/vcvars64. Binaries land in
`build/<config>/bin/Voxcraft.exe`.

## Architecture

Read `docs/ARCHITECTURE.md` before structural changes — it has the layering
rules and the milestone roadmap. The short version:

- `engine/src/vox/` — reusable engine (namespace `vox`), modules `core/`,
  `platform/`, `renderer/`. Never includes game code.
- `game/src/` — Voxcraft gameplay; never calls OpenGL directly, only
  `vox::Renderer`.
- Fixed-timestep loop: `OnTick(dt)` at 20 TPS for simulation, `OnRender(alpha,
  frameDt)` uncapped for drawing.

## Conventions

- Includes root-relative: `#include "vox/core/Log.h"`.
- `PascalCase` types/methods, `m_member`, `s_static`. 4-space indent,
  100 cols (`.clang-format`).
- Logging: `VOX_INFO(...)` in engine code, `GAME_INFO(...)` in game code.
  Asserts: `VOX_ASSERT(cond, "message")` (debug-only).
- New engine source files must be added to `engine/CMakeLists.txt` (explicit
  file lists, no globbing).
