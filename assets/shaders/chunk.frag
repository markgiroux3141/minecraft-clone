#version 460 core

in vec3 v_normal;
in vec3 v_uvw;
in vec3 v_worldPos;
in float v_ao;
in float v_sky;
in float v_block;

uniform sampler2DArray u_atlas;
uniform vec3 u_sunDir;    // toward the dominant body: sun by day, moon by night
uniform float u_sunLight; // scales sky light over the day (moonlight floor)
uniform vec3 u_skyTint;   // skylight color: white by day, cool blue at night
uniform vec3 u_eyePos;
uniform vec3 u_fogColor;
uniform vec2 u_fogRange; // (start, end) distance

out vec4 o_color;

void main() {
    vec3 normal = normalize(v_normal);
    float diffuse = max(dot(normal, normalize(u_sunDir)), 0.0);

    // Sky light carries the directional sun term (caves get no sun) and
    // tracks the time of day; block light is warm, omnidirectional, and
    // constant — glowstone holds its ground at night. AO darkens both.
    vec3 skyTerm = u_skyTint * (v_sky * u_sunLight * (0.40 + 0.60 * diffuse));
    vec3 light = max(skyTerm, vec3(1.0, 0.85, 0.62) * v_block);
    light = max(light, vec3(0.03)); // never fully black

    float ao = mix(0.4, 1.0, v_ao);
    vec4 albedo = texture(u_atlas, v_uvw);
    vec3 color = albedo.rgb * ao * light;

    // Distance fog toward the horizon color; underwater the caller hands
    // in a short range and deep blue instead.
    float dist = distance(v_worldPos, u_eyePos);
    float fog = clamp((dist - u_fogRange.x) / (u_fogRange.y - u_fogRange.x), 0.0, 1.0);
    // Alpha passes through: opaque pass draws with blending off, the water
    // pass blends with the texture's alpha.
    o_color = vec4(mix(color, u_fogColor, fog), albedo.a);
}
