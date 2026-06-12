#version 460 core

in vec2 v_uv;

uniform sampler2D u_texture;
uniform vec3 u_tint; // fades the body out below the horizon

out vec4 o_color;

void main() {
    // Drawn additively: the sheets' black backgrounds contribute nothing.
    vec4 tex = texture(u_texture, v_uv);
    o_color = vec4(tex.rgb * u_tint, tex.a);
}
