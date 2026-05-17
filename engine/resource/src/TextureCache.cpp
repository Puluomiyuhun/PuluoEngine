#include "puluo/resource/TextureCache.h"
#include "puluo/core/Log.h"

#include <algorithm>

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

    auto texture = std::make_shared<Texture2D>(path);
    if (texture->GetRendererID() == 0) {
        return nullptr;
    }

    s_Cache[key] = texture;
    PULUO_CORE_INFO("TextureCache miss, loaded: {}", key);
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
