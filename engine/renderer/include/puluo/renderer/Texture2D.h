#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace Puluo {

struct TextureSpec {
    bool generateMips = true;
};

class AssetLoader; // Forward declaration

class Texture2D {
public:
    Texture2D(const std::string& filepath);
    Texture2D(int width, int height, const void* data = nullptr);
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    void Bind(uint32_t slot = 0) const;

    void SetData(const void* data, uint32_t size);

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    int GetChannels() const { return m_Channels; }
    uint32_t GetRendererID() const { return m_RendererID; }

    bool operator==(const Texture2D& other) const { return m_RendererID == other.m_RendererID; }

    // Load from encoded image data in memory (e.g. PNG/JPG buffer from GLB)
    static std::shared_ptr<Texture2D> CreateFromMemory(const unsigned char* data, int length);

private:
    friend class AssetLoader;
    uint32_t m_RendererID = 0;
    int m_Width = 0, m_Height = 0, m_Channels = 0;
    uint32_t m_InternalFormat = 0, m_DataFormat = 0;
};

} // namespace Puluo
