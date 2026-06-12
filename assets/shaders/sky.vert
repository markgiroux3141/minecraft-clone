#version 460 core

layout(location = 0) in vec2 a_position; // fullscreen quad in NDC

out vec2 v_ndc;

void main() {
    v_ndc = a_position;
    // Depth 0 with depth writes off: the sky never occludes anything and
    // the terrain drawn afterwards covers it.
    gl_Position = vec4(a_position, 0.0, 1.0);
}
