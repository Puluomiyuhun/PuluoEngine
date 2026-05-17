#pragma once

#include <string>

namespace Puluo {

class AssetImporter {
public:
    // ---- Import-to-project API (导入制管线，外部资产 → 项目内 .passet) ----

    // 导入模型：外部 .glb/.fbx/.obj → assets/models/xxx.passet
    static std::string ImportModelToProject(
        const std::string& externalPath,
        const std::string& destDir = "assets/models");

    // 导入纹理：外部 .png/.jpg/.tga/.bmp/.hdr → assets/textures/xxx.passet
    static std::string ImportTextureToProject(
        const std::string& externalPath,
        const std::string& destDir = "assets/textures");

    // ---- Legacy cache API (向后兼容旧 .pscene 的自动缓存) ----

    // Import a raw model file and write a .passet binary cache.
    static std::string Import(const std::string& sourcePath, const std::string& cacheDir = "assets/cache");

    // Import a standalone texture file and write a .passet binary cache.
    static std::string ImportTexture(const std::string& sourcePath, const std::string& cacheDir = "assets/cache");

    // Compute the expected .passet cache path for a given source path.
    static std::string GetCachePath(const std::string& sourcePath, const std::string& cacheDir = "assets/cache");

private:
    // Shared implementation: decode texture and write .passet to outPath
    static std::string WriteTexturePAsset(const std::string& sourcePath, const std::string& outPath);

    // Shared implementation: parse model with Assimp and write .passet to outPath
    static std::string WriteModelPAsset(const std::string& sourcePath, const std::string& outPath);
};

} // namespace Puluo
