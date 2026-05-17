#pragma once

#include "puluo/renderer/Model.h"
#include <string>
#include <memory>
#include <unordered_map>

namespace Puluo {

class ModelCache {
public:
    // Load model by file path (cache hit returns existing instance)
    static std::shared_ptr<Model> Load(const std::string& path);

    // Clear all cached entries
    static void Clear();

    // Number of live cached entries
    static size_t GetCachedCount();

private:
    static std::string NormalizePath(const std::string& path);

    static std::unordered_map<std::string, std::weak_ptr<Model>> s_Cache;
};

} // namespace Puluo
