#version 460 core

layout(location = 0) in vec3 a_position; // unit cube
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in float a_face; // BlockFace index

uniform mat4 u_viewProj;
uniform vec3 u_center;          // world-space cube center
uniform float u_scale;          // cube edge length (1 falling block, 0.25 item)
uniform float u_yaw;            // spin around the vertical axis, radians
uniform float u_faceLayers[6];  // texture-array layer per face

out vec3 v_normal;
out vec3 v_uvw;

void main() {
    float c = cos(u_yaw);
    float s = sin(u_yaw);
    mat3 spin = mat3(c, 0.0, -s, 0.0, 1.0, 0.0, s, 0.0, c);
    v_normal = spin * a_normal;
    v_uvw = vec3(a_uv, u_faceLayers[int(a_face + 0.5)]);
    vec3 world = spin * (a_position - vec3(0.5)) * u_scale + u_center;
    gl_Position = u_viewProj * vec4(world, 1.0);
}
