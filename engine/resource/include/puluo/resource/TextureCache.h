#pragma once

#include "puluo/renderer/Texture2D.h"
#include <string>
#include <memory>
#include <unordered_map>

namespace Puluo {

class TextureCache {
public:
    // Load texture by file path (cache hit returns existing instance)
    static std::shared_ptr<Texture2D> Load(const std::string& path);

    // Load texture from memory data (embedded textures, keyed by unique string)
    static std::shared_ptr<Texture2D> LoadFromMemory(
        const std::string& key,
        const unsigned char* data, int length);

    // Clear all cached entries
    static void Clear();

    // Number of live cached entries
    static size_t GetCachedCount();

private:
    // Normalize path: forward slashes, remove leading ./
    static std::string NormalizePath(const std::string& path);

    // weak_ptr so textures are freed when no longer referenced
    static std::unordered_map<std::string, std::weak_ptr<Texture2D>> s_Cache;
};

} // namespace Puluo
