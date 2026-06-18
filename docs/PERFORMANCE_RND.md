# Performance — speculative / R&D ideas

Written: 2026-06-18. This is the **frontier brainstorm**, deliberately
inventive — not a work plan. For the actual ranked, shippable plan see
[PERFORMANCE.md](PERFORMANCE.md). Honest framing: the boring Step 1/2 in that
doc will out-deliver everything here per hour spent. These are graded by how
novel they are and how well they fit *this* engine's existing structures (light
BFS at [LightEngine.cpp:63](../game/src/world/LightEngine.cpp#L63), runtime
occlusion flood at [World.cpp:1324](../game/src/world/World.cpp#L1324), LOD loop
at [World.cpp:986](../game/src/world/World.cpp#L986), MDI at
[MeshPool.cpp](../engine/src/vox/renderer/MeshPool.cpp)).

**If forced to pick one to try first: #1 (precomputed portal graph)** — reuses
data we already produce, removes per-frame work, unlocks much longer view
distance, and slots into the occlusion machinery we already have.

## Grounded recombinations (uncommon, but buildable)

### 1. Auto-derived portal graph for occlusion
Precompute what we currently flood at runtime. We already do a per-voxel BFS
connectivity flood every frame. Instead, **at mesh time** (the padded volume is
already in hand), compute a tiny **6×6 face-to-face connectivity matrix** per
chunk — "can air connect entry-face A to exit-face B inside this chunk?" (15
bits). Runtime visibility becomes pure graph reachability over chunk portals
from the camera chunk — no per-voxel work per frame, scales to far larger view
distances. It's Minecraft's cave-culling idea, but *precomputed and compressed
into the mesh job* rather than re-flooded live. Recompute the matrix only on
edits, scoped like the existing remesh.
- **Fit:** excellent — extends the existing BFS occlusion + remesh scoping.
- **Catch:** matrix must be invalidated/recomputed on edits (cheap).

### 2. Light via 3D Jump Flooding (JFA) on the GPU
Sky + block light are multi-source distance fields with per-step attenuation and
opacity barriers. JFA computes approximate distance fields in O(log n) parallel
passes — move it to a compute shader over the chunk brick and the entire CPU
`ThreadPool` is freed for meshing/gen, killing the most serial part of the sim.
JFA is common in 2D GI / SDF text; not seen for voxel light.
- **Fit:** good — replaces `LightEngine` column BFS, frees CPU workers.
- **Catch:** JFA is approximate around 1-wide barriers; needs a correction pass
  or two. Light is visually forgiving, so the error budget is generous.

### 3. GPU compute meshing — upload voxels, not vertices
A 16³ chunk is ~4 KB of raw 8-bit IDs vs. a much larger vertex buffer, and we're
throttled by a 2 MB/frame *upload* cap. Upload the raw volume and run greedy
meshing in a compute shader writing directly into the MDI buffers. Upload
bottleneck mostly evaporates; CPU freed. Exists in a few hobby engines, not
mainstream.
- **Fit:** good — feeds the existing MDI buffers.
- **Catch:** GPU greedy needs prefix-sum compaction; AO/light sampling crosses
  chunk borders, so neighbor volumes must be resident.

## The radical rewrite (huge payoff, whole-renderer change)

### 4. Mesh nothing — raymarch a sparse-voxel brickmap
Store voxels in a GPU SVO/brickmap and raymarch in a compute/fragment shader
(Teardown / John-Lin direction). Deletes meshing, LOD, uploads, *and* per-chunk
draws in one stroke — cost becomes screen-resolution-bound ray traversal, plus
temporal accumulation + checkerboard to amortize. Biggest possible win.
- **Fit:** none — this is "engine v2," not an optimization of this one.

## Genuinely speculative — no prior art found

### 5. The world as a pure function + sparse edit overlay (canonical, not a flag)
Terrain is already deterministic gen (`TerrainGen`/`CaveGen`/`OreGen`/`LakeGen`
over `JavaRandom`). Make the canonical world representation *the function itself*
plus a sparse map of **only player edits**. Pristine chunks are never stored or
persisted — regenerated identically on revisit; their meshes (even LOD meshes)
disk-cached keyed by `seed + gen-version`. Save files become pure diffs; an
infinite world costs almost nothing where untouched. MC tracks "is this chunk
modified" as a flag; pushing it to *"function + diff is the only source of
truth"* is the inventive part.
- **Catch:** gen must stay perfectly deterministic across versions; cross-chunk
  features (trees, lakes straddling borders) need careful edit semantics.

### 6. Motion-aware / foveated sim budgeting
At high camera angular velocity the player can't resolve fine detail — so spend
less: coarser/deferred meshing and lighting during fast turns, reclaim that
worker budget, refine when the view settles. Foveation applied to *simulation
update rate* rather than shading resolution. The perceptual headroom is real.
- **Catch:** refinement must settle without visible "sharpening" pops.

### 7. Cull-decision caching via open-face signatures
Hash each chunk's open-face signature; when the camera moves sub-chunk and no
neighbor signatures changed, reuse last frame's visibility set (and transparent
sort order) wholesale. Most frames the scene topology is identical — we're
recomputing a function of unchanged inputs. Pairs naturally with #1.
- **Catch:** invalidate on any neighbor signature change; correctness hinges on
  a sound signature.
