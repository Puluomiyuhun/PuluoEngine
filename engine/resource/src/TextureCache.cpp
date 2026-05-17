#include "puluo/resource/TextureCache.h"
#include "puluo/resource/AssetImporter.h"
#include "puluo/resource/AssetLoader.h"
#include "puluo/core/Log.h"

#include <algorithm>
#include <filesystem>

namespace Puluo {

std::unordered_map<std::string, std::weak_ptr<Texture2D>> TextureCache::s_Cache;

std::string TextureCache::NormalizePath(const std::string& path) {
    std::string result = path;

    // Backslash -> forward slash
    std::replace(result.begin(), result.end(), '\\', '/');

    // Strip leading "./"
    while (result.size() >= 2 && result[0] == '.' && result[1] == '/') {
        result = result.substr(2);
    }

    return result;
}

// Returns true for relative paths only (absolute paths like D:/... or /... are not cacheable)
static bool IsRelativePath(const std::string& path) {
    if (path.empty()) return false;
    if (path[0] == '/') return false;
    if (path.size() >= 2 && path[1] == ':') return false; // Windows drive letter
    return true;
}

std::shared_ptr<Texture2D> TextureCache::Load(const std::string& path) {
    std::string key = NormalizePath(path);

    auto it = s_Cache.find(key);
    if (it != s_Cache.end()) {
        if (auto existing = it->second.lock()) {
            PULUO_CORE_TRACE("TextureCache hit: {}", key);
            return existing;
        }
        // Expired, remove stale entry
        s_Cache.erase(it);
    }

    // .passet files: load directly via AssetLoader (no stbi, no timestamp check)
    if (key.size() >= 7 && key.substr(key.size() - 7) == ".passet") {
        auto texture = AssetLoader::LoadTexture(key);
        if (texture) {
            s_Cache[key] = texture;
            return texture;
        }
        PULUO_CORE_ERROR("TextureCache: failed to load .passet: {}", key);
        return nullptr;
    }

    // Non-.passet relative paths: try disk cache with timestamp validation
    bool canDiskCache = IsRelativePath(key);
    if (canDiskCache) {
        std::string cachePath = AssetImporter::GetCachePath(key);

        bool cacheValid = false;
        if (std::filesystem::exists(cachePath) && std::filesystem::exists(key)) {
            auto cacheTime = std::filesystem::last_write_time(cachePath);
            auto sourceTime = std::filesystem::last_write_time(key);
            cacheValid = (cacheTime >= sourceTime);
        }

        if (cacheValid) {
            auto texture = AssetLoader::LoadTexture(cachePath);
            if (texture) {
                s_Cache[key] = texture;
                PULUO_CORE_INFO("TextureCache: loaded from .passet cache: {}", cachePath);
                return texture;
            }
            PULUO_CORE_WARN("TextureCache: .passet cache invalid, falling back to original: {}", key);
        }
    }

    // Original stbi load
    auto texture = std::make_shared<Texture2D>(path);
    if (texture->GetRendererID() == 0) {
        return nullptr;
    }

    s_Cache[key] = texture;
    PULUO_CORE_INFO("TextureCache miss, loaded: {}", key);

    // Auto-generate .passet cache for next time (only relative project paths)
    if (canDiskCache && key.size() >= 4) {
        std::string ext = key.substr(key.rfind('.'));
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".tga" || ext == ".bmp" || ext == ".hdr") {
            std::string cachePath = AssetImporter::ImportTexture(key);
            if (!cachePath.empty()) {
                PULUO_CORE_INFO("TextureCache: generated .passet cache: {}", cachePath);
            }
        }
    }

    return texture;
}

std::shared_ptr<Texture2D> TextureCache::LoadFromMemory(
    const std::string& key,
    const unsigned char* data, int length)
{
    auto it = s_Cache.find(key);
    if (it != s_Cache.end()) {
        if (auto existing = it->second.lock()) {
            PULUO_CORE_TRACE("TextureCache hit (memory): {}", key);
            return existing;
        }
        s_Cache.erase(it);
    }

    auto texture = Texture2D::CreateFromMemory(data, length);
    if (!texture) {
        return nullptr;
    }

    s_Cache[key] = texture;
    return texture;
}

void TextureCache::Clear() {
    size_t count = s_Cache.size();
    s_Cache.clear();
    PULUO_CORE_INFO("TextureCache cleared ({} entries)", count);
}

size_t TextureCache::GetCachedCount() {
    // Purge expired entries and return live count
    size_t live = 0;
    for (auto it = s_Cache.begin(); it != s_Cache.end(); ) {
        if (it->second.expired()) {
            it = s_Cache.erase(it);
        } else {
            ++live;
            ++it;
        }
    }
    return live;
}

} // namespace Puluo
