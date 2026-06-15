#include "vox/renderer/Framebuffer.h"

#include <stdexcept>

#include <glad/gl.h>

#include "vox/renderer/Texture.h"

namespace vox {

Framebuffer::Framebuffer(uint32_t width, uint32_t height) : m_width(width), m_height(height) {
    m_color = std::make_shared<Texture2D>(width, height);

    glCreateRenderbuffers(1, &m_depth);
    glNamedRenderbufferStorage(m_depth, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(width),
                               static_cast<GLsizei>(height));

    glCreateFramebuffers(1, &m_handle);
    glNamedFramebufferTexture(m_handle, GL_COLOR_ATTACHMENT0, m_color->RendererId(), 0);
    glNamedFramebufferRenderbuffer(m_handle, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth);

    if (glCheckNamedFramebufferStatus(m_handle, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("Framebuffer incomplete");
    }
}

Framebuffer::~Framebuffer() {
    glDeleteFramebuffers(1, &m_handle);
    glDeleteRenderbuffers(1, &m_depth);
}

void Framebuffer::Bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
}

void Framebuffer::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace vox
