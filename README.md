# Voxcraft

A Minecraft-style voxel game built from scratch in C++23 / OpenGL 4.6, with a
modular custom engine (`vox`). See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
for the design and milestone roadmap.

## Requirements

- Windows, Visual Studio 2026 (or newer) with the C++ workload
- CMake 3.28+
- Internet access on first configure (dependencies are fetched and pinned)

## Build & run

```powershell
.\scripts\build.ps1            # debug build
.\scripts\build.ps1 -Run       # build and launch
.\scripts\build.ps1 -Config release
```

The script wraps `cmake --preset debug|release` in a VS developer environment.
Opening the folder directly in Visual Studio (CMake folder mode) also works.

The executable lands in `build/<config>/bin/Voxcraft.exe`.

## Dependencies

| Library | Purpose | How |
| ------- | ------- | --- |
| GLFW 3.4 | window + input | FetchContent |
| glad (GL 4.6 core) | GL loader | vendored in `third_party/glad` |
| GLM 1.0.1 | math | FetchContent |
| spdlog 1.15.3 | logging | FetchContent |

To regenerate glad: `pip install glad2`, then
`python -m glad --api "gl:core=4.6" --out-path third_party/glad c`.
