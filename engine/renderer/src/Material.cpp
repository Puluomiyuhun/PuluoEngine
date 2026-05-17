#include "puluo/renderer/Material.h"

namespace Puluo {

Material::Material(std::shared_ptr<Shader> shader)
    : m_Shader(std::move(shader)) {}

void Material::Bind() const {
    m_Shader->Bind();

    for (const auto& [name, value] : m_Uniforms) {
        std::visit([&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, float>)
                m_Shader->SetFloat(name, v);
            else if constexpr (std::is_same_v<T, Vec3>)
                m_Shader->SetVec3(name, v);
            else if constexpr (std::is_same_v<T, Vec4>)
                m_Shader->SetVec4(name, v);
            else if constexpr (std::is_same_v<T, Mat4>)
                m_Shader->SetMat4(name, v);
        }, value);
    }

    for (const auto& [name, binding] : m_Textures) {
        binding.texture->Bind(binding.slot);
        m_Shader->SetInt(name, static_cast<int>(binding.slot));
    }
}

void Material::Unbind() const {
    m_Shader->Unbind();
}

void Material::Set(const std::string& name, float value) {
    m_Uniforms[name] = value;
}

void Material::Set(const std::string& name, const Vec3& value) {
    m_Uniforms[name] = value;
}

void Material::Set(const std::string& name, const Vec4& value) {
    m_Uniforms[name] = value;
}

void Material::Set(const std::string& name, const Mat4& value) {
    m_Uniforms[name] = value;
}

void Material::SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture, uint32_t slot) {
    m_Textures[name] = {std::move(texture), slot};
}

} // namespace Puluo
