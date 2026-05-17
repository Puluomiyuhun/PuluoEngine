#include "puluo/resource/ModelCache.h"
#include "puluo/resource/AssetImporter.h"
#include "puluo/resource/AssetLoader.h"
#include "puluo/core/Log.h"

#include <algorithm>
#include <filesystem>

namespace Puluo {

std::unordered_map<std::string, std::weak_ptr<Model>> ModelCache::s_Cache;

std::string ModelCache::NormalizePath(const std::string& path) {
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
    if (path.size() >= 2 && path[1] == ':') return false;
    return true;
}

std::shared_ptr<Model> ModelCache::Load(const std::string& path) {
    std::string key = NormalizePath(path);

    // Memory cache check
    auto it = s_Cache.find(key);
    if (it != s_Cache.end()) {
        if (auto existing = it->second.lock()) {
            PULUO_CORE_TRACE("ModelCache hit: {}", key);
            return existing;
        }
        s_Cache.erase(it);
    }

    std::shared_ptr<Model> model;

    // Try .passet binary cache (only for relative project paths, skip .passet files)
    bool canDiskCache = IsRelativePath(key) &&
                        (key.size() < 7 || key.substr(key.size() - 7) != ".passet");
    if (canDiskCache) {
        std::string cachePath = AssetImporter::GetCachePath(key);

        bool cacheValid = false;
        if (std::filesystem::exists(cachePath) && std::filesystem::exists(key)) {
            auto cacheTime = std::filesystem::last_write_time(cachePath);
            auto sourceTime = std::filesystem::last_write_time(key);
            cacheValid = (cacheTime >= sourceTime);
        }

        if (cacheValid) {
            model = AssetLoader::LoadModel(cachePath);
            if (model) {
                s_Cache[key] = model;
                PULUO_CORE_INFO("ModelCache: loaded from .passet cache: {}", cachePath);
                return model;
            }
            // Cache file corrupted, fall through to original load
            PULUO_CORE_WARN("ModelCache: .passet cache invalid, falling back to original: {}", key);
        }
    }

    // Original Assimp load
    model = std::make_shared<Model>();
    if (!model->Load(path)) {
        PULUO_CORE_ERROR("ModelCache: failed to load '{}'", key);
        return nullptr;
    }

    s_Cache[key] = model;
    PULUO_CORE_INFO("ModelCache miss, loaded: {}", key);

    // Auto-generate .passet cache for next time (only relative project paths)
    if (canDiskCache && key.size() >= 4) {
        std::string ext = key.substr(key.size() - 4);
        // Only cache known model formats
        if (ext == ".glb" || ext == "gltf" || ext == ".fbx" || ext == ".obj") {
            std::string cachePath = AssetImporter::Import(key);
            if (!cachePath.empty()) {
                PULUO_CORE_INFO("ModelCache: generated .passet cache: {}", cachePath);
            }
        }
    }

    return model;
}

void ModelCache::Clear() {
    size_t count = s_Cache.size();
    s_Cache.clear();
    PULUO_CORE_INFO("ModelCache cleared ({} entries)", count);
}

size_t ModelCache::GetCachedCount() {
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
