#version 460 core

// Block-break particles: corners are billboarded on the CPU (camera
// right/up), so the vertex shader is a pure pass-through projection.
layout(location = 0) in vec3 a_position; // world space
layout(location = 1) in vec3 a_uvw;      // tile-local uv + atlas layer
layout(location = 2) in vec2 a_light;    // sky/block light at spawn, 0..1

uniform mat4 u_viewProj;

out vec3 v_uvw;
out vec2 v_light;

void main() {
    v_uvw = a_uvw;
    v_light = a_light;
    gl_Position = u_viewProj * vec4(a_position, 1.0);
}
