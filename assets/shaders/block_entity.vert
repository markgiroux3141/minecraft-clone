#version 460 core

layout(location = 0) in vec3 a_position; // unit cube
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in float a_face; // BlockFace index

uniform mat4 u_viewProj;
uniform vec3 u_origin;          // world-space min corner of the block
uniform float u_faceLayers[6];  // texture-array layer per face

out vec3 v_normal;
out vec3 v_uvw;

void main() {
    v_normal = a_normal;
    v_uvw = vec3(a_uv, u_faceLayers[int(a_face + 0.5)]);
    gl_Position = u_viewProj * vec4(a_position + u_origin, 1.0);
}
