add_library(vox_engine STATIC
    src/vox/audio/AudioEngine.cpp
    src/vox/audio/AudioEngine.h
    src/vox/core/Application.cpp
    src/vox/core/Application.h
    src/vox/core/Assert.h
    src/vox/core/Assets.cpp
    src/vox/core/Assets.h
    src/vox/core/Log.cpp
    src/vox/core/Log.h
    src/vox/core/ThreadPool.cpp
    src/vox/core/ThreadPool.h
    src/vox/platform/Input.cpp
    src/vox/platform/Input.h
    src/vox/platform/KeyCodes.h
    src/vox/platform/Window.cpp
    src/vox/platform/Window.h
    src/vox/renderer/Buffer.cpp
    src/vox/renderer/Buffer.h
    src/vox/renderer/Camera.cpp
    src/vox/renderer/Camera.h
    src/vox/renderer/Frustum.h
    src/vox/renderer/MeshPool.cpp
    src/vox/renderer/MeshPool.h
    src/vox/renderer/Renderer.cpp
    src/vox/renderer/Renderer.h
    src/vox/renderer/Shader.cpp
    src/vox/renderer/Shader.h
    src/vox/renderer/Texture.cpp
    src/vox/renderer/Texture.h
    src/vox/renderer/TextureArray.cpp
    src/vox/renderer/TextureArray.h
    src/vox/renderer/UiRenderer.cpp
    src/vox/renderer/UiRenderer.h
    src/vox/renderer/VertexArray.cpp
    src/vox/renderer/VertexArray.h
)
add_library(vox::engine ALIAS vox_engine)

target_include_directories(vox_engine PUBLIC src)

target_link_libraries(vox_engine
    PUBLIC glad glm::glm spdlog::spdlog
    PRIVATE glfw stb miniaudio
)

# Keep GLFW from dragging system GL headers into translation units;
# glad/gl.h is the single source of GL declarations.
target_compile_definitions(vox_engine PRIVATE GLFW_INCLUDE_NONE)

vox_set_target_defaults(vox_engine)
