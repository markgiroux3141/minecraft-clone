#version 460 core

in vec3 v_normal;
in vec3 v_uvw;

uniform sampler2DArray u_atlas; // unit 0: block tiles + item sprites
uniform sampler2D u_skin;       // unit 1: player skin (bare arm)
uniform float u_useSkin;        // 1 = sample u_skin with v_uvw.xy
uniform vec2 u_light;           // sky/block light at the eye, 0..1
uniform float u_sunLight;
uniform vec3 u_skyTint;

out vec4 o_color;

void main() {
    vec4 tex = mix(texture(u_atlas, v_uvw), texture(u_skin, v_uvw.xy), u_useSkin);
    if (tex.a < 0.5) {
        discard;
    }
    // Fixed view-space key light from the upper-left so the cube/arm read
    // as 3D regardless of where the player faces; intensity still follows
    // the world's light at the eye.
    float diffuse = max(dot(normalize(v_normal), normalize(vec3(-0.35, 0.9, 0.45))), 0.0);
    vec3 skyTerm = u_skyTint * (u_light.x * u_sunLight * (0.55 + 0.45 * diffuse));
    vec3 light = max(skyTerm, vec3(1.0, 0.85, 0.62) * u_light.y * (0.55 + 0.45 * diffuse));
    light = max(light, vec3(0.05));
    o_color = vec4(tex.rgb * light, 1.0);
}
