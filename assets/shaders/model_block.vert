#version 460 core

// Fat model vertex (see ModelVertex in ChunkMesher.h) — the float-position
// stream for non-cube blocks (torches now, slabs/stairs later). Feeds the
// exact same varyings as chunk.vert, so the two streams share chunk.frag.
//   a_pos:    chunk-local block units (cell + fractional offset)
//   a_uv:     tile-space UV (1.0 = one tile; sub-rects sample within a tile)
//   a_packed: layer:16 | normal:3 | ao:4 | sky:4 | block:4
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in uint a_packed;

uniform mat4 u_viewProj;

// Per-draw chunk transform, same contract as chunk.vert (xyz = world-space
// chunk min corner, w = uniform scale). Model blocks only exist in
// full-detail chunks, so w is always 1, but the layout is shared.
layout(std430, binding = 0) readonly buffer PerDraw {
    vec4 u_perDraw[];
};

out vec3 v_normal;
out vec3 v_uvw;
out vec3 v_worldPos;
out float v_ao;
out float v_sky;
out float v_block;

// BlockFace order: +X, -X, +Y, -Y, +Z, -Z.
const vec3 kNormals[6] = vec3[](vec3(1, 0, 0), vec3(-1, 0, 0), vec3(0, 1, 0), vec3(0, -1, 0),
                                vec3(0, 0, 1), vec3(0, 0, -1));

void main() {
    v_uvw = vec3(a_uv, float(a_packed & 0xFFFFu));
    v_normal = kNormals[(a_packed >> 16) & 7u];
    v_ao = float((a_packed >> 19) & 15u) / 15.0;
    v_sky = float((a_packed >> 23) & 15u) / 15.0;
    v_block = float((a_packed >> 27) & 15u) / 15.0;
    v_worldPos = a_pos * u_perDraw[gl_DrawID].w + u_perDraw[gl_DrawID].xyz;
    gl_Position = u_viewProj * vec4(v_worldPos, 1.0);
}
