#pragma once

#include <cstdint>
#include <string>

namespace Puluo {

class Cubemap {
public:
    // Create empty cubemap (for FBO rendering)
    Cubemap(uint32_t size, bool hdr = true, bool mipmap = false);
    ~Cubemap();

    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;

    void Bind(uint32_t slot = 0) const;
    uint32_t GetRendererID() const { return m_RendererID; }
    uint32_t GetSize() const { return m_Size; }

    void GenerateMipmaps() const;

private:
    uint32_t m_RendererID = 0;
    uint32_t m_Size = 0;
};

} // namespace Puluo
