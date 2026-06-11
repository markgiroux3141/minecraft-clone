#version 460 core

in vec3 v_normal;
in vec3 v_uvw;
in float v_ao;
in float v_sky;
in float v_block;

uniform sampler2DArray u_atlas;

out vec4 o_color;

void main() {
    vec3 normal = normalize(v_normal);
    vec3 lightDir = normalize(vec3(0.6, 1.0, 0.4));
    float diffuse = max(dot(normal, lightDir), 0.0);

    // Sky light carries the directional sun term (caves get no sun);
    // block light is warm and omnidirectional. AO darkens both.
    float skyTerm = v_sky * (0.40 + 0.60 * diffuse);
    vec3 light = max(vec3(skyTerm), vec3(1.0, 0.85, 0.62) * v_block);
    light = max(light, vec3(0.03)); // never fully black

    float ao = mix(0.4, 1.0, v_ao);
    vec3 albedo = texture(u_atlas, v_uvw).rgb;
    o_color = vec4(albedo * ao * light, 1.0);
}
