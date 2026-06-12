#version 460 core

// Reuses the sky pass's fullscreen-quad vertex buffer as a unit quad: the
// corner is placed on a billboard far along u_dir, in eye-relative space
// (rotation-only view), so the body stays glued to the sky.
layout(location = 0) in vec2 a_position; // quad corner, -1..1

uniform mat4 u_viewRotProj; // projection * rotation-only view
uniform vec3 u_dir;         // unit direction toward the body
uniform vec3 u_right;       // billboard basis, pre-scaled by the half-size
uniform vec3 u_up;
uniform vec2 u_uvMin; // sub-rect of the sheet (moon phases); sun uses 0..1
uniform vec2 u_uvMax;

out vec2 v_uv;

void main() {
    v_uv = mix(u_uvMin, u_uvMax, a_position * 0.5 + 0.5);
    const float kDistance = 100.0;
    vec3 world = u_dir * kDistance + u_right * a_position.x + u_up * a_position.y;
    gl_Position = u_viewRotProj * vec4(world, 1.0);
}
