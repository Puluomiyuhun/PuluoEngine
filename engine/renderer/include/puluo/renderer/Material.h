#pragma once

#include "puluo/core/Math.h"
#include "puluo/renderer/Shader.h"
#include "puluo/renderer/Texture2D.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>

namespace Puluo {

class Material {
public:
    Material(std::shared_ptr<Shader> shader);

    void Bind() const;
    void Unbind() const;

    void Set(const std::string& name, float value);
    void Set(const std::string& name, const Vec3& value);
    void Set(const std::string& name, const Vec4& value);
    void Set(const std::string& name, const Mat4& value);
    void SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture, uint32_t slot);

    std::shared_ptr<Shader> GetShader() const { return m_Shader; }

private:
    std::shared_ptr<Shader> m_Shader;

    using UniformValue = std::variant<float, Vec3, Vec4, Mat4>;
    std::unordered_map<std::string, UniformValue> m_Uniforms;

    struct TextureBinding {
        std::shared_ptr<Texture2D> texture;
        uint32_t slot;
    };
    std::unordered_map<std::string, TextureBinding> m_Textures;
};

} // namespace Puluo
