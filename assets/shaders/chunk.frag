#version 460 core

in vec3 v_normal;
in vec3 v_uvw;
in float v_ao;

uniform sampler2DArray u_atlas;

out vec4 o_color;

void main() {
    vec3 normal = normalize(v_normal);
    vec3 lightDir = normalize(vec3(0.6, 1.0, 0.4));
    float diffuse = max(dot(normal, lightDir), 0.0);
    float ao = mix(0.4, 1.0, v_ao); // baked corner AO, 0..1 from the mesher
    vec3 albedo = texture(u_atlas, v_uvw).rgb;
    o_color = vec4(albedo * ao * (0.35 + 0.65 * diffuse), 1.0);
}
