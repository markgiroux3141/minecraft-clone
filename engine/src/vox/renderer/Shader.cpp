#include "vox/renderer/Shader.h"

#include <stdexcept>
#include <vector>

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include "vox/core/Assets.h"
#include "vox/core/Log.h"

namespace vox {

namespace {

GLuint CompileStage(GLenum stage, std::string_view source, const std::string& shaderName) {
    const GLuint handle = glCreateShader(stage);
    const GLchar* src = source.data();
    const GLint length = static_cast<GLint>(source.size());
    glShaderSource(handle, 1, &src, &length);
    glCompileShader(handle);

    GLint success = GL_FALSE;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<GLchar> log(static_cast<size_t>(logLength) + 1, '\0');
        glGetShaderInfoLog(handle, logLength, nullptr, log.data());
        glDeleteShader(handle);

        const char* stageName = (stage == GL_VERTEX_SHADER) ? "vertex" : "fragment";
        VOX_ERROR("{} shader '{}' failed to compile:\n{}", stageName, shaderName, log.data());
        throw std::runtime_error("Shader compilation failed: " + shaderName);
    }
    return handle;
}

} // namespace

Shader::Shader(std::string name, std::string_view vertexSrc, std::string_view fragmentSrc)
    : m_name(std::move(name)) {
    const GLuint vertex = CompileStage(GL_VERTEX_SHADER, vertexSrc, m_name);
    const GLuint fragment = CompileStage(GL_FRAGMENT_SHADER, fragmentSrc, m_name);

    m_handle = glCreateProgram();
    glAttachShader(m_handle, vertex);
    glAttachShader(m_handle, fragment);
    glLinkProgram(m_handle);
    glDetachShader(m_handle, vertex);
    glDetachShader(m_handle, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint success = GL_FALSE;
    glGetProgramiv(m_handle, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetProgramiv(m_handle, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<GLchar> log(static_cast<size_t>(logLength) + 1, '\0');
        glGetProgramInfoLog(m_handle, logLength, nullptr, log.data());
        glDeleteProgram(m_handle);

        VOX_ERROR("Shader program '{}' failed to link:\n{}", m_name, log.data());
        throw std::runtime_error("Shader link failed: " + m_name);
    }
}

Shader::~Shader() {
    glDeleteProgram(m_handle);
}

std::shared_ptr<Shader> Shader::FromFiles(std::string_view vertexPath,
                                          std::string_view fragmentPath) {
    return std::make_shared<Shader>(std::string(vertexPath), assets::ReadText(vertexPath),
                                    assets::ReadText(fragmentPath));
}

void Shader::Bind() const {
    glUseProgram(m_handle);
}

int Shader::UniformLocation(std::string_view name) {
    const auto it = m_uniformCache.find(std::string(name));
    if (it != m_uniformCache.end()) {
        return it->second;
    }

    const std::string key(name);
    const GLint location = glGetUniformLocation(m_handle, key.c_str());
    if (location == -1) {
        VOX_WARN("Shader '{}': uniform '{}' not found (optimized out?)", m_name, key);
    }
    m_uniformCache.emplace(key, location);
    return location;
}

void Shader::SetInt(std::string_view name, int value) {
    glProgramUniform1i(m_handle, UniformLocation(name), value);
}

void Shader::SetFloat(std::string_view name, float value) {
    glProgramUniform1f(m_handle, UniformLocation(name), value);
}

void Shader::SetFloat2(std::string_view name, const glm::vec2& value) {
    glProgramUniform2fv(m_handle, UniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetFloat3(std::string_view name, const glm::vec3& value) {
    glProgramUniform3fv(m_handle, UniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetFloat4(std::string_view name, const glm::vec4& value) {
    glProgramUniform4fv(m_handle, UniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetMat4(std::string_view name, const glm::mat4& value) {
    glProgramUniformMatrix4fv(m_handle, UniformLocation(name), 1, GL_FALSE,
                              glm::value_ptr(value));
}

} // namespace vox
