#version 460 core

in vec2 v_ndc;

uniform mat4 u_invViewProj;
uniform vec3 u_eyePos;
uniform vec3 u_zenithColor;
uniform vec3 u_horizonColor;
uniform vec3 u_sunDir;
uniform vec3 u_sunColor; // black when the sun is fully set
uniform float u_disc;    // 0 when a textured sun is drawn instead

out vec4 o_color;

void main() {
    // Unproject the far-plane point under this pixel to get the view ray.
    vec4 far = u_invViewProj * vec4(v_ndc, 1.0, 1.0);
    vec3 ray = normalize(far.xyz / far.w - u_eyePos);

    float up = clamp(ray.y, 0.0, 1.0);
    vec3 color = mix(u_horizonColor, u_zenithColor, pow(up, 0.65));

    float sunDot = max(dot(ray, u_sunDir), 0.0);
    color += u_sunColor * pow(sunDot, 350.0) * 1.2 * u_disc; // procedural disc
    color += u_sunColor * pow(sunDot, 8.0) * 0.12;           // halo (kept always)

    o_color = vec4(color, 1.0);
}
