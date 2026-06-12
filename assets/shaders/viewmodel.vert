#version 460 core

// First-person view model (held block/tool, bare arm). u_model is a full
// VIEW-SPACE transform (vanilla's first-person matrices port over as-is);
// only the projection is applied on top.
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in float a_face; // BlockFace index (atlas mode)

uniform mat4 u_proj;
uniform mat4 u_model;
uniform float u_faceLayers[6];

out vec3 v_normal; // view space
out vec3 v_uvw;

void main() {
    v_normal = mat3(u_model) * a_normal;
    v_uvw = vec3(a_uv, u_faceLayers[int(a_face + 0.5)]);
    gl_Position = u_proj * u_model * vec4(a_position, 1.0);
}
