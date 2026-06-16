#version 460 core

in vec3 v_normal;
in vec2 v_uv;

uniform sampler2D u_skin;
uniform vec3 u_sunDir;   // toward the dominant body: sun by day, moon by night
uniform float u_sunLight;
uniform vec3 u_skyTint;
uniform vec2 u_light;    // sky/block light sampled at the entity's cell, 0..1

out vec4 o_color;

void main() {
    // Same lighting model as block_entity.frag (one sampled light value for
    // the whole entity, no fog). Skin overlay layers (hat) are transparent —
    // alpha-test them out like the chunk/entity cutout pass.
    vec4 tex = texture(u_skin, v_uv);
    if (tex.a < 0.5) {
        discard;
    }
    float diffuse = max(dot(normalize(v_normal), normalize(u_sunDir)), 0.0);
    vec3 skyTerm = u_skyTint * (u_light.x * u_sunLight * (0.40 + 0.60 * diffuse));
    vec3 light = max(skyTerm, vec3(1.0, 0.85, 0.62) * u_light.y);
    light = max(light, vec3(0.03));
    o_color = vec4(tex.rgb * light, 1.0);
}
