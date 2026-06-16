#version 460 core

layout(location = 0) in vec3 a_position; // pixel-space, part-local
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;       // 0..1 in the skin

uniform mat4 u_viewProj;
uniform mat4 u_model; // per-part: modelToWorld * accumulated local transform

out vec3 v_normal;
out vec2 v_uv;

void main() {
    // u_model folds in the 1/16 scale and the orientation flip, so its linear
    // part flips normals along with the geometry — they stay outward-facing.
    v_normal = mat3(u_model) * a_normal;
    v_uv = a_uv;
    gl_Position = u_viewProj * u_model * vec4(a_position, 1.0);
}
