#version 460 core

// Packed vertex (see ChunkVertex in ChunkMesher.h):
//   a_data0: x:5 | y:5 | z:5 | normal:3 | ao:2 | sky:4 | block:4 | xIn:2 | zIn:2
//            (xIn/zIn: sub-block insets, torch planes only)
//   a_data1: u:5 | v:5 | layer:16 | drop:4 (liquid surface, ninths)
layout(location = 0) in uint a_data0;
layout(location = 1) in uint a_data1;

uniform mat4 u_viewProj;

// Per-draw chunk transform, filled by vox::MeshPool::Draw and indexed by
// the multi-draw's gl_DrawID: xyz = world-space chunk min corner, w =
// uniform scale (1 for full-detail chunks, 2 for half-res LOD chunks
// whose vertices are in cell units).
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

// Sub-block inset codes (torch planes sit 7/16 and 9/16 into the cell).
const float kInset[4] = float[](0.0, 7.0 / 16.0, 9.0 / 16.0, 0.0);

void main() {
    vec3 position = vec3(float(a_data0 & 31u), float((a_data0 >> 5) & 31u),
                         float((a_data0 >> 10) & 31u));
    position.y -= float((a_data1 >> 26) & 15u) / 9.0; // liquid surface drop
    position.x += kInset[(a_data0 >> 28) & 3u];
    position.z += kInset[a_data0 >> 30];
    v_normal = kNormals[(a_data0 >> 15) & 7u];
    v_ao = float((a_data0 >> 18) & 3u) / 3.0;
    v_sky = float((a_data0 >> 20) & 15u) / 15.0;
    v_block = float((a_data0 >> 24) & 15u) / 15.0;
    v_uvw = vec3(float(a_data1 & 31u), float((a_data1 >> 5) & 31u),
                 float((a_data1 >> 10) & 0xFFFFu));
    v_worldPos = position * u_perDraw[gl_DrawID].w + u_perDraw[gl_DrawID].xyz;
    gl_Position = u_viewProj * vec4(v_worldPos, 1.0);
}
