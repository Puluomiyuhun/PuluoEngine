#pragma once

#include <string>

namespace Puluo {

class AssetImporter {
public:
    // Import a raw model file (glTF/FBX/OBJ) and write a .passet binary cache.
    // Returns the output .passet path, or empty string on failure.
    // cacheDir: directory to write cache files (e.g. "assets/cache")
    static std::string Import(const std::string& sourcePath, const std::string& cacheDir = "assets/cache");

    // Compute the expected .passet cache path for a given source path.
    static std::string GetCachePath(const std::string& sourcePath, const std::string& cacheDir = "assets/cache");
};

} // namespace Puluo
