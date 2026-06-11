#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <glm/glm.hpp>

namespace vox {

class Shader {
public:
    // Sources are GLSL text; name appears in error logs.
    Shader(std::string name, std::string_view vertexSrc, std::string_view fragmentSrc);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Loads from asset-relative paths, e.g. "shaders/cube.vert".
    static std::shared_ptr<Shader> FromFiles(std::string_view vertexPath,
                                             std::string_view fragmentPath);

    void Bind() const;

    void SetInt(std::string_view name, int value);
    void SetFloat(std::string_view name, float value);
    void SetFloat3(std::string_view name, const glm::vec3& value);
    void SetFloat4(std::string_view name, const glm::vec4& value);
    void SetMat4(std::string_view name, const glm::mat4& value);

    const std::string& Name() const { return m_name; }

private:
    int UniformLocation(std::string_view name);

    uint32_t m_handle = 0;
    std::string m_name;
    std::unordered_map<std::string, int> m_uniformCache;
};

} // namespace vox
