#pragma once

namespace vox {

// Values match GLFW key codes so the platform layer can pass them through
// directly. Extend as needed; the full table lives in GLFW/glfw3.h.
enum class Key : int {
    Space = 32,
    Num0 = 48,
    Num1 = 49,
    Num2 = 50,
    Num3 = 51,
    Num4 = 52,
    Num5 = 53,
    Num6 = 54,
    Num7 = 55,
    Num8 = 56,
    Num9 = 57,
    A = 65,
    B = 66,
    C = 67,
    D = 68,
    E = 69,
    F = 70,
    G = 71,
    H = 72,
    J = 74,
    K = 75,
    M = 77,
    N = 78,
    O = 79,
    Q = 81,
    R = 82,
    S = 83,
    T = 84,
    V = 86,
    W = 87,
    Escape = 256,
    Enter = 257,
    Tab = 258,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    F1 = 290,
    F2 = 291,
    F3 = 292,
    F4 = 293,
    F11 = 300,
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
};

enum class MouseButton : int {
    Left = 0,
    Right = 1,
    Middle = 2,
};

} // namespace vox
