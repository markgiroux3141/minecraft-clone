#include "vox/renderer/Renderer.h"

#include <glad/gl.h>

#include "vox/core/Log.h"
#include "vox/renderer/VertexArray.h"

namespace vox {

namespace {

#ifdef VOX_DEBUG
void GLAD_API_PTR GlDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                  GLsizei /*length*/, const GLchar* message,
                                  const void* /*userParam*/) {
    (void)source;
    (void)type;
    (void)id;
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        VOX_ERROR("[GL] {}", message);
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        VOX_WARN("[GL] {}", message);
        break;
    case GL_DEBUG_SEVERITY_LOW:
        VOX_INFO("[GL] {}", message);
        break;
    default:
        VOX_TRACE("[GL] {}", message);
        break;
    }
}
#endif

} // namespace

void Renderer::Init() {
    VOX_INFO("OpenGL {} | {} | {}",
             reinterpret_cast<const char*>(glGetString(GL_VERSION)),
             reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
             reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

#ifdef VOX_DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(GlDebugCallback, nullptr);
    // Notification-severity spam (buffer usage hints etc.) stays off; the
    // callback still receives everything LOW and above.
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                          GL_FALSE);
#endif

    glEnable(GL_DEPTH_TEST);
    // Voxel rendering depends on backface culling; meshes must wind CCW.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

void Renderer::Shutdown() {}

void Renderer::SetClearColor(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
}

void Renderer::Clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::DrawIndexed(const VertexArray& vertexArray, uint32_t indexCount) {
    vertexArray.Bind();
    const uint32_t count = indexCount ? indexCount : vertexArray.GetIndexBuffer()->Count();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT, nullptr);
}

void Renderer::DrawLines(const VertexArray& vertexArray, uint32_t indexCount) {
    vertexArray.Bind();
    const uint32_t count = indexCount ? indexCount : vertexArray.GetIndexBuffer()->Count();
    glDrawElements(GL_LINES, static_cast<GLsizei>(count), GL_UNSIGNED_INT, nullptr);
}

void Renderer::SetBlend(BlendMode mode) {
    switch (mode) {
    case BlendMode::None:
        glDisable(GL_BLEND);
        break;
    case BlendMode::Alpha:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case BlendMode::Additive:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;
    }
}

void Renderer::SetDepthWrite(bool enabled) {
    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void Renderer::SetCullFace(bool enabled) {
    if (enabled) {
        glEnable(GL_CULL_FACE);
    } else {
        glDisable(GL_CULL_FACE);
    }
}

void Renderer::SetViewport(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return; // minimized
    }
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

} // namespace vox
