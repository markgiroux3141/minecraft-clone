#version 460 core

in vec3 v_normal;
in vec3 v_uvw;

uniform sampler2DArray u_atlas;

out vec4 o_color;

void main() {
    vec3 normal = normalize(v_normal);
    vec3 lightDir = normalize(vec3(0.6, 1.0, 0.4));
    float diffuse = max(dot(normal, lightDir), 0.0);
    vec3 albedo = texture(u_atlas, v_uvw).rgb;
    o_color = vec4(albedo * (0.35 + 0.65 * diffuse), 1.0);
}
