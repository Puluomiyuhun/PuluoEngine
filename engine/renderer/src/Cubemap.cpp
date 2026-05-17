#include "puluo/renderer/Cubemap.h"
#include <glad/gl.h>

namespace Puluo {

Cubemap::Cubemap(uint32_t size, bool hdr, bool mipmap)
    : m_Size(size)
{
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

    for (uint32_t i = 0; i < 6; i++) {
        if (hdr)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT, nullptr);
        else
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    if (mipmap) {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    } else {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
}

Cubemap::~Cubemap() {
    if (m_RendererID)
        glDeleteTextures(1, &m_RendererID);
}

void Cubemap::Bind(uint32_t slot) const {
    glBindTextureUnit(slot, m_RendererID);
}

void Cubemap::GenerateMipmaps() const {
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
}

} // namespace Puluo
