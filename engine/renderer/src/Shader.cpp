#include "puluo/renderer/Shader.h"
#include "puluo/core/Base.h"
#include "puluo/core/Log.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>

namespace Puluo {

Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
    uint32_t vertShader = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    uint32_t fragShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);

    m_RendererID = glCreateProgram();
    glAttachShader(m_RendererID, vertShader);
    glAttachShader(m_RendererID, fragShader);
    glLinkProgram(m_RendererID);

    int success;
    glGetProgramiv(m_RendererID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_RendererID, 512, nullptr, infoLog);
        PULUO_CORE_ERROR("Shader link error: {0}", infoLog);
        glDeleteProgram(m_RendererID);
        m_RendererID = 0;
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
}

Shader::Shader(const std::string& vertexSrc, const std::string& tescSrc,
               const std::string& teseSrc, const std::string& fragmentSrc) {
    uint32_t vertShader = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    uint32_t tescShader = CompileShader(GL_TESS_CONTROL_SHADER, tescSrc);
    uint32_t teseShader = CompileShader(GL_TESS_EVALUATION_SHADER, teseSrc);
    uint32_t fragShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);

    m_RendererID = glCreateProgram();
    glAttachShader(m_RendererID, vertShader);
    glAttachShader(m_RendererID, tescShader);
    glAttachShader(m_RendererID, teseShader);
    glAttachShader(m_RendererID, fragShader);
    glLinkProgram(m_RendererID);

    int success;
    glGetProgramiv(m_RendererID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_RendererID, 512, nullptr, infoLog);
        PULUO_CORE_ERROR("Shader link error: {0}", infoLog);
        glDeleteProgram(m_RendererID);
        m_RendererID = 0;
    }

    glDeleteShader(vertShader);
    glDeleteShader(tescShader);
    glDeleteShader(teseShader);
    glDeleteShader(fragShader);
}

Shader::~Shader() {
    if (m_RendererID)
        glDeleteProgram(m_RendererID);
}

void Shader::Bind() const {
    glUseProgram(m_RendererID);
}

void Shader::Unbind() const {
    glUseProgram(0);
}

int Shader::GetUniformLocation(const std::string& name) const {
    auto it = m_UniformLocationCache.find(name);
    if (it != m_UniformLocationCache.end())
        return it->second;

    int location = glGetUniformLocation(m_RendererID, name.c_str());
    if (location == -1)
        PULUO_CORE_WARN("Uniform '{0}' not found in shader", name);
    m_UniformLocationCache[name] = location;
    return location;
}

void Shader::SetInt(const std::string& name, int value) {
    glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetFloat(const std::string& name, float value) {
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetVec2(const std::string& name, const Vec2& value) {
    glUniform2f(GetUniformLocation(name), value.x, value.y);
}

void Shader::SetVec3(const std::string& name, const Vec3& value) {
    glUniform3f(GetUniformLocation(name), value.x, value.y, value.z);
}

void Shader::SetVec4(const std::string& name, const Vec4& value) {
    glUniform4f(GetUniformLocation(name), value.x, value.y, value.z, value.w);
}

void Shader::SetMat3(const std::string& name, const Mat3& value) {
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetMat4(const std::string& name, const Mat4& value) {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetFloatArray(const std::string& name, const float* values, uint32_t count) {
    glUniform1fv(GetUniformLocation(name), count, values);
}

void Shader::SetMat4Array(const std::string& name, const Mat4* values, uint32_t count) {
    glUniformMatrix4fv(GetUniformLocation(name), count, GL_FALSE, glm::value_ptr(values[0]));
}

std::string Shader::ReadFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        PULUO_CORE_ERROR("Failed to open shader file: {0}", filepath);
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

uint32_t Shader::CompileShader(uint32_t type, const std::string& source) {
    uint32_t shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        const char* typeName = "UNKNOWN";
        switch (type) {
            case GL_VERTEX_SHADER:          typeName = "VERTEX"; break;
            case GL_FRAGMENT_SHADER:        typeName = "FRAGMENT"; break;
            case GL_TESS_CONTROL_SHADER:    typeName = "TESS_CONTROL"; break;
            case GL_TESS_EVALUATION_SHADER: typeName = "TESS_EVALUATION"; break;
        }
        PULUO_CORE_ERROR("{0} shader compile error: {1}", typeName, infoLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

std::shared_ptr<Shader> Shader::CreateFromFile(const std::string& vertPath, const std::string& fragPath) {
    std::string vertSrc = ReadFile(vertPath);
    std::string fragSrc = ReadFile(fragPath);
    if (vertSrc.empty() || fragSrc.empty())
        return nullptr;

    auto shader = std::make_shared<Shader>(vertSrc, fragSrc);

    // Extract name from file path (e.g. "assets/shaders/basic.vert" -> "basic")
    auto lastSlash = vertPath.find_last_of("/\\");
    auto lastDot = vertPath.rfind('.');
    if (lastSlash == std::string::npos) lastSlash = 0; else lastSlash++;
    shader->SetName(vertPath.substr(lastSlash, lastDot - lastSlash));

    PULUO_CORE_INFO("Shader '{0}' compiled successfully", shader->GetName());
    return shader;
}

std::shared_ptr<Shader> Shader::CreateFromFile(
    const std::string& vertPath, const std::string& tescPath,
    const std::string& tesePath, const std::string& fragPath) {
    std::string vertSrc = ReadFile(vertPath);
    std::string tescSrc = ReadFile(tescPath);
    std::string teseSrc = ReadFile(tesePath);
    std::string fragSrc = ReadFile(fragPath);
    if (vertSrc.empty() || tescSrc.empty() || teseSrc.empty() || fragSrc.empty())
        return nullptr;

    auto shader = std::make_shared<Shader>(vertSrc, tescSrc, teseSrc, fragSrc);

    auto lastSlash = vertPath.find_last_of("/\\");
    auto lastDot = vertPath.rfind('.');
    if (lastSlash == std::string::npos) lastSlash = 0; else lastSlash++;
    shader->SetName(vertPath.substr(lastSlash, lastDot - lastSlash));

    PULUO_CORE_INFO("Tessellation shader '{0}' compiled successfully", shader->GetName());
    return shader;
}

} // namespace Puluo
