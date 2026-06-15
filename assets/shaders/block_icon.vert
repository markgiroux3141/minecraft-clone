#version 460 core

// Bakes a block's 3D model into an inventory icon. u_mvp is the full
// clip-space transform (ortho projection * the vanilla GUI display transform:
// rotate [30,225,0], scale 0.625). a_face indexes the per-face atlas layer.
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in float a_face;

uniform mat4 u_mvp;
uniform float u_faceLayers[6];

out vec3 v_uvw;
out vec3 v_normal;

void main() {
    v_uvw = vec3(a_uv, u_faceLayers[int(a_face + 0.5)]);
    v_normal = a_normal;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
