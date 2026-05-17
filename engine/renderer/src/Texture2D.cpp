#include "puluo/renderer/Texture2D.h"
#include "puluo/core/Base.h"
#include "puluo/core/Log.h"

#include <glad/gl.h>
#include "stb/stb_image.h"
#include <cmath>
#include <algorithm>

namespace Puluo {

Texture2D::Texture2D(const std::string& filepath) {
    stbi_set_flip_vertically_on_load(1);

    int channels;
    unsigned char* data = stbi_load(filepath.c_str(), &m_Width, &m_Height, &channels, 0);
    if (!data) {
        PULUO_CORE_ERROR("Failed to load texture: {0}", filepath);
        return;
    }
    m_Channels = channels;

    if (channels == 4) {
        m_InternalFormat = GL_RGBA8;
        m_DataFormat = GL_RGBA;
    } else if (channels == 3) {
        m_InternalFormat = GL_RGB8;
        m_DataFormat = GL_RGB;
    } else if (channels == 1) {
        m_InternalFormat = GL_R8;
        m_DataFormat = GL_RED;
    }

    int mipLevels = static_cast<int>(std::floor(std::log2(std::max(m_Width, m_Height)))) + 1;

    glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
    glTextureStorage2D(m_RendererID, mipLevels, m_InternalFormat, m_Width, m_Height);

    glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Anisotropic filtering — greatly reduces texture shimmer at glancing angles
    float maxAniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    if (maxAniso > 0.0f)
        glTextureParameterf(m_RendererID, GL_TEXTURE_MAX_ANISOTROPY, std::min(maxAniso, 16.0f));

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glGenerateTextureMipmap(m_RendererID);

    stbi_image_free(data);

    PULUO_CORE_INFO("Texture loaded: {0} ({1}x{2}, {3}ch)", filepath, m_Width, m_Height, channels);
}

Texture2D::Texture2D(int width, int height, const void* data)
    : m_Width(width), m_Height(height)
{
    m_InternalFormat = GL_RGBA8;
    m_DataFormat = GL_RGBA;

    glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
    glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);

    glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (data)
        glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
}

Texture2D::~Texture2D() {
    if (m_RendererID)
        glDeleteTextures(1, &m_RendererID);
}

void Texture2D::Bind(uint32_t slot) const {
    glBindTextureUnit(slot, m_RendererID);
}

void Texture2D::SetData(const void* data, uint32_t size) {
    glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
}

std::shared_ptr<Texture2D> Texture2D::CreateFromMemory(const unsigned char* data, int length) {
    if (!data || length <= 0) {
        PULUO_CORE_WARN("CreateFromMemory: invalid data or length");
        return nullptr;
    }

    stbi_set_flip_vertically_on_load(1);

    int width, height, channels;
    unsigned char* pixels = stbi_load_from_memory(data, length, &width, &height, &channels, 0);
    if (!pixels) {
        PULUO_CORE_WARN("Failed to load embedded texture from memory");
        return nullptr;
    }

    if (width <= 0 || height <= 0 || channels <= 0 || channels > 4) {
        stbi_image_free(pixels);
        return nullptr;
    }

    auto tex = std::make_shared<Texture2D>(width, height, nullptr);
    tex->m_Channels = channels;
    if (channels == 4) {
        tex->m_InternalFormat = GL_RGBA8;
        tex->m_DataFormat = GL_RGBA;
    } else if (channels == 3) {
        tex->m_InternalFormat = GL_RGB8;
        tex->m_DataFormat = GL_RGB;
    } else if (channels == 2) {
        tex->m_InternalFormat = GL_RG8;
        tex->m_DataFormat = GL_RG;
    } else if (channels == 1) {
        tex->m_InternalFormat = GL_R8;
        tex->m_DataFormat = GL_RED;
    }

    // Recreate storage with correct format and full mip chain
    glDeleteTextures(1, &tex->m_RendererID);
    glCreateTextures(GL_TEXTURE_2D, 1, &tex->m_RendererID);
    int mipLevels = static_cast<int>(std::floor(std::log2(std::max(width, height)))) + 1;
    glTextureStorage2D(tex->m_RendererID, mipLevels, tex->m_InternalFormat, width, height);

    glTextureParameteri(tex->m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(tex->m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(tex->m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(tex->m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

    float maxAniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    if (maxAniso > 0.0f)
        glTextureParameterf(tex->m_RendererID, GL_TEXTURE_MAX_ANISOTROPY, std::min(maxAniso, 16.0f));

    // stbi provides tightly packed rows — set alignment to 1 for non-4-aligned row sizes
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(tex->m_RendererID, 0, 0, 0, width, height, tex->m_DataFormat, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // restore default
    glGenerateTextureMipmap(tex->m_RendererID);

    stbi_image_free(pixels);

    PULUO_CORE_INFO("Embedded texture loaded: {0}x{1}, {2}ch", width, height, channels);
    return tex;
}

} // namespace Puluo
