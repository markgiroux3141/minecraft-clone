#version 460 core

in vec3 v_uvw;
in vec3 v_normal;

uniform sampler2DArray u_atlas; // block tiles

out vec4 o_color;

void main() {
    vec4 tex = texture(u_atlas, v_uvw);
    if (tex.a < 0.5) {
        discard;
    }
    // Vanilla's per-face block shading (EnumFacing diffuse): top full bright,
    // bottom darkest, the N/S vs E/W sides at distinct mid tones — this is
    // what makes a flat-colored cube read as 3D in the GUI.
    vec3 n = normalize(v_normal);
    float shade = 0.8; // north/south default
    if (n.y > 0.5) {
        shade = 1.0; // up
    } else if (n.y < -0.5) {
        shade = 0.5; // down
    } else if (abs(n.x) > 0.5) {
        shade = 0.6; // east/west
    }
    o_color = vec4(tex.rgb * shade, tex.a);
}
