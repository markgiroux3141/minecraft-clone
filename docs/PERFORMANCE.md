# Performance — investigation & optimization plan

Written: 2026-06-18, for a FRESH CONTEXT. This is an investigation snapshot +
a ranked work plan, not work in progress. Nothing here is started yet.

## TL;DR

The engine is already past the "naive voxel engine" stage. There is **no big win
left from a graphics-API rewrite** (DX12/Vulkan would buy ~0–5% and cost weeks —
see "Why not DX12" below). The chunk render path is already submission-light;
the only remaining waste is in **entity rendering** and one **worldgen LOD loop**.

**Do this, in order:**
1. Add a frame profiler (measure before touching anything).
2. Entity/mob render batching (the only naive per-frame hot path left).
3. Fix the O(count²) LOD downsampling loop in worldgen.

All cost figures below are **unprofiled estimates** from a code read — confirm
with the profiler (step 1) before optimizing. Today the only instrumentation is
FPS/TPS in the window title + an `m_chunksDrawn` counter
([GameApp.cpp:1792](../game/src/GameApp.cpp#L1792)); there is no per-system timing.

## What's already good (leave it alone)

- **Greedy meshing** (merges coplanar faces), not per-face quads — [ChunkMesher.cpp:645](../game/src/world/ChunkMesher.cpp#L645)
- **One `glMultiDrawElementsIndirect` per pass** for all chunks via SSBO + `gl_DrawID`, not one draw per chunk — [MeshPool.cpp:238](../engine/src/vox/renderer/MeshPool.cpp#L238)
- **Frustum + BFS occlusion culling** (face-connectivity flood from camera chunk) — [World.cpp:1324](../game/src/world/World.cpp#L1324)
- **Threaded gen/light/mesh** on `vox::ThreadPool`, nearest-first in-flight budget (`ThreadCount * kInFlightPerWorker`), 2 MB/frame GPU upload cap — [World.cpp:1623](../game/src/world/World.cpp#L1623)
- **Copy-on-write neighbor snapshots** — workers mesh from a stack-local padded volume, zero locks — [World.cpp:838](../game/src/world/World.cpp#L838)
- **Surgically scoped remeshing** — a block edit dirties only the touched chunk + affected neighbors; light re-floods only columns within reach — [World.cpp:241](../game/src/world/World.cpp#L241)

Chunk facts: 16³ chunks; `ChunkVertex` is 8 bytes packed; meshing/lighting/gen
all async and versioned (stale results dropped).

## The work plan

### Step 0 — Frame profiler (do first, small & safe)

Without per-system timing we'll optimize the wrong thing. Add scoped
`std::chrono` timers around the tick subsystems and a render-pass timer, surface
them in an F3-style debug overlay (the stats plumbing already half-exists at
[GameApp.cpp:1777](../game/src/GameApp.cpp#L1777), `m_statsTimer`/`m_chunksDrawn`).

Key question the profiler answers, which decides what to do next:
- **Low average FPS** → do step 1 (entity batching).
- **Frame-time hitching/spikes** → do step 2 (LOD) and look at lighting / the
  upload budget.

### Step 1 — Entity & mob render batching (best perf-per-effort)

Terrain is ~2–3 draw calls/frame; entities are ~20–100. The entity path is the
only naive one left:
- Every falling block / item / mob box-part is a separate `DrawIndexed` with the
  VAO **rebound every call** — [Renderer.cpp:75](../engine/src/vox/renderer/Renderer.cpp#L75)
- 8–10 uniforms re-set **per instance** — items/falling blocks at
  [GameApp.cpp:1573-1614](../game/src/GameApp.cpp#L1573), mobs at
  [GameApp.cpp:1697-1732](../game/src/GameApp.cpp#L1697)
- Box-model parts: one `DrawIndexed` per part (~10 per humanoid) —
  `BoxModel::Render()` in [engine/src/vox/renderer/BoxModel.cpp](../engine/src/vox/renderer/BoxModel.cpp)

Plan: instance the entity cubes (one draw, per-instance data in an SSBO indexed
by `gl_InstanceID`, mirroring the chunk MDI approach), and batch box-model parts.
Matters most exactly when it matters most — crowded scenes. Effort: moderate,
contained to the render layer + entity draw code; no architecture change.

### Step 2 — LOD column downsampling (worst algorithm, isolated)

[World.cpp:986-1029](../game/src/world/World.cpp#L986) does O(count²) mode-finding
with 3–5 `BlockRegistry::Def()` lookups **per cell** across ~16K cells/column.
Causes hitches when LOD columns generate during fast travel.

Fix: hoist `const auto& registry = BlockRegistry::Get();` out of the loop;
replace the quadratic frequency count with a small histogram. Low effort,
isolated to one function.

### Step 3 — Smooth lighting / upload spikes (only if profiler shows hitching)

Light BFS ([LightEngine.cpp:63](../game/src/world/LightEngine.cpp#L63)) and the
fixed 2 MB/frame upload cap are the likely sources of stutter on world load /
fast flight. If the profiler shows frame-time spikes (not low average FPS),
consider a per-frame *time* budget for uploads rather than a fixed byte budget.

### Minor / skip unless the profiler flags them

- Per-frame transparent-chunk sort ([World.cpp:1276](../game/src/world/World.cpp#L1276)) — trivial at realistic water counts.
- Greedy-merge row scans ([ChunkMesher.cpp:657](../game/src/world/ChunkMesher.cpp#L657)) — already cheap.

## Why not DX12 (asked & answered)

GL is fully encapsulated behind `vox::Renderer` (~9 files, ~2,140 lines, ~148 GL
call sites in [engine/src/vox/renderer/](../engine/src/vox/renderer/); game code
never calls GL), so a backend swap is *contained* — but not *worth it*:
- **Perf:** ~0–5%, possibly negative until hand-tuned. DX12 wins on CPU
  driver/submission overhead, which is **not** this game's bottleneck (CPU sim +
  GPU bandwidth are). The chunk path is already MDI-batched.
- **Effort:** ~4–8 focused weeks for a DX12-fluent dev — explicit PSOs, root
  signatures, descriptor heaps, fences, barriers, upload heaps, plus porting ~18
  GLSL shaders ([assets/shaders/](../assets/shaders/)) to HLSL, plus a long tail
  of visual-correctness bugs.

If a modern explicit API is ever wanted for *learning/portability* (not perf),
Vulkan is comparable effort and keeps cross-platform. Neither is justified by
performance for this title.
