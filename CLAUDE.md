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
  file lists, no globbing). New game source files go in `game/CMakeLists.txt`
  (keep the list alphabetized).

## Where new code goes (read before adding files)

Do NOT default to dropping a new `Foo.h/.cpp` at the root of `game/src/`. Pick
the folder by what the thing *is*:

| If it's…                                                  | Put it in            | Namespace |
|-----------------------------------------------------------|----------------------|-----------|
| Reusable, game-agnostic (could ship in another game)      | `engine/src/vox/...` | `vox`     |
| A simulated body (player, mob, projectile, dropped item)  | `game/src/entity/`   | `vc`      |
| An entity's box-model / skin renderer                     | `game/src/entity/`   | `vc`      |
| Items, inventory, crafting, recipes                       | `game/src/item/`     | `vc`      |
| Game-side render helpers (view model, particles, icons)   | `game/src/render/`   | `vc`      |
| Voxel sim: chunks, meshing, lighting, worldgen, blocks,   | `game/src/world/`    | `vc`      |
| &nbsp;&nbsp;block entities, persistence                   |                      |           |
| A screen, menu, or HUD widget                             | `game/src/ui/`       | `vc`      |
| Mapping game events → sounds                              | `game/src/audio/`    | `vc`      |
| App wiring only (state machine, the OnTick/OnRender shell)| `game/src/` (root)   | (global)  |

Only `GameApp` and `Main` live at the root of `game/src/`. If a new file would
land at the root and it isn't app-shell wiring, it's in the wrong place.

Content (a new block / item / mob / recipe) should almost always be *data* added
to an existing registry — `BlockRegistry`/`BlockDef`, `ItemRegistry`/`ItemDef`,
`kMobDefs`/`MobDef`, the recipe registry — NOT a new code path. If a new content
type needs new behavior, add the behavior to the owning subsystem, then express
the instance as data.

## Keeping modules small (avoid god objects)

`GameApp` and `World` are the two files most likely to accrete one-off features
because they orchestrate everything. Resist it:

- **Before adding a member to `GameApp` or `World`**, ask whether it belongs in a
  subsystem. More than ~2 related fields/methods = it's a new class, not more
  members on the god object.
- **Shared entity state** (prev/current position, velocity, AABB, the
  render-interpolation + walk-cycle accumulators) lives on the `vc::Entity` base
  in `game/src/entity/Entity.h`. Do not re-declare those fields on a new entity —
  inherit them.
- **Soft size budget: ~800 lines per source file.** `scripts\check_sizes.ps1`
  flags files over budget. When you cross it, split by responsibility before
  adding more. (`GameApp.cpp` and `World.cpp` are the legacy exceptions being
  carved down — don't grow them; extract instead.)
