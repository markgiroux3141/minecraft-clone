#version 460 core

in vec3 v_uvw;
in vec2 v_light;

uniform sampler2DArray u_atlas;
uniform float u_sunLight;
uniform vec3 u_skyTint;

out vec4 o_color;

void main() {
    vec4 tex = texture(u_atlas, v_uvw);
    if (tex.a < 0.5) {
        discard;
    }
    // Flat-lit like vanilla particles; the 0.6 gray is ParticleDigging's
    // chip tint.
    vec3 light = max(u_skyTint * (v_light.x * u_sunLight), vec3(1.0, 0.85, 0.62) * v_light.y);
    light = max(light, vec3(0.03));
    o_color = vec4(tex.rgb * 0.6 * light, 1.0);
}
