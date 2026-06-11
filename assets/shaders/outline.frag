#version 460 core

out vec4 o_color;

void main() {
    // Light gray reads against dirt/stone shadow; near-black didn't.
    o_color = vec4(0.92, 0.92, 0.92, 1.0);
}
