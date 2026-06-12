#version 460 core

in vec3 v_normal;
in vec3 v_uvw;

uniform sampler2DArray u_atlas;
uniform vec3 u_sunDir;
uniform float u_sunLight;
uniform vec2 u_light; // sky/block light sampled at the entity's cell, 0..1

out vec4 o_color;

void main() {
    // Same lighting model as chunk.frag, minus per-vertex AO/light (the
    // whole entity shares one sampled light value) and fog (entities are
    // always near the player).
    float diffuse = max(dot(normalize(v_normal), normalize(u_sunDir)), 0.0);
    float skyTerm = u_light.x * u_sunLight * (0.40 + 0.60 * diffuse);
    vec3 light = max(vec3(skyTerm), vec3(1.0, 0.85, 0.62) * u_light.y);
    light = max(light, vec3(0.03));
    o_color = vec4(texture(u_atlas, v_uvw).rgb * light, 1.0);
}
