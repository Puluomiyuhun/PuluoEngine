#pragma once

#include "puluo/core/Math.h"
#include <string>
#include <memory>
#include <unordered_map>

namespace Puluo {

class Shader {
public:
    Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
    // Tessellation pipeline: vert + tesc + tese + frag
    Shader(const std::string& vertexSrc, const std::string& tescSrc,
           const std::string& teseSrc, const std::string& fragmentSrc);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void Bind() const;
    void Unbind() const;

    void SetInt(const std::string& name, int value);
    void SetFloat(const std::string& name, float value);
    void SetVec2(const std::string& name, const Vec2& value);
    void SetVec3(const std::string& name, const Vec3& value);
    void SetVec4(const std::string& name, const Vec4& value);
    void SetMat3(const std::string& name, const Mat3& value);
    void SetMat4(const std::string& name, const Mat4& value);
    void SetFloatArray(const std::string& name, const float* values, uint32_t count);
    void SetMat4Array(const std::string& name, const Mat4* values, uint32_t count);

    uint32_t GetRendererID() const { return m_RendererID; }
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    static std::shared_ptr<Shader> CreateFromFile(
        const std::string& vertPath, const std::string& fragPath);

    // Tessellation pipeline from files
    static std::shared_ptr<Shader> CreateFromFile(
        const std::string& vertPath, const std::string& tescPath,
        const std::string& tesePath, const std::string& fragPath);

private:
    uint32_t m_RendererID = 0;
    std::string m_Name;
    mutable std::unordered_map<std::string, int> m_UniformLocationCache;

    int GetUniformLocation(const std::string& name) const;
    static std::string ReadFile(const std::string& filepath);
    static uint32_t CompileShader(uint32_t type, const std::string& source);
};

} // namespace Puluo
