#version 460 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in float a_layer;

uniform mat4 u_viewProj;
uniform mat4 u_model;

out vec3 v_normal;
out vec3 v_uvw;

void main() {
    v_normal = mat3(u_model) * a_normal;
    v_uvw = vec3(a_uv, a_layer);
    gl_Position = u_viewProj * u_model * vec4(a_position, 1.0);
}
