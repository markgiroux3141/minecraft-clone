#version 460 core

in vec3 v_normal;
in vec3 v_uvw;

uniform sampler2DArray u_atlas;
uniform vec3 u_sunDir; // toward the dominant body: sun by day, moon by night
uniform float u_sunLight;
uniform vec3 u_skyTint;
uniform vec2 u_light; // sky/block light sampled at the entity's cell, 0..1

out vec4 o_color;

void main() {
    // Same lighting model as chunk.frag, minus per-vertex AO/light (the
    // whole entity shares one sampled light value) and fog (entities are
    // always near the player). Alpha holes are cut out like the chunk
    // pass — item minis of leaves/plants and the crack overlay rely on it.
    vec4 tex = texture(u_atlas, v_uvw);
    if (tex.a < 0.5) {
        discard;
    }
    float diffuse = max(dot(normalize(v_normal), normalize(u_sunDir)), 0.0);
    vec3 skyTerm = u_skyTint * (u_light.x * u_sunLight * (0.40 + 0.60 * diffuse));
    vec3 light = max(skyTerm, vec3(1.0, 0.85, 0.62) * u_light.y);
    light = max(light, vec3(0.03));
    o_color = vec4(tex.rgb * light, 1.0);
}
