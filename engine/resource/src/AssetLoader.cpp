#include "puluo/resource/AssetLoader.h"
#include "puluo/resource/AssetFormat.h"
#include "puluo/renderer/Texture2D.h"
#include "puluo/renderer/Mesh.h"
#include "puluo/core/Log.h"

#include <glad/gl.h>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>

namespace Puluo {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
template<typename T>
static bool ReadVal(std::ifstream& f, T& val) {
    f.read(reinterpret_cast<char*>(&val), sizeof(T));
    return f.good();
}

static bool ReadBytes(std::ifstream& f, void* dst, size_t size) {
    f.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(size));
    return f.good();
}

// Create a Texture2D from raw decoded pixels via friend access (no stbi, no file IO)
std::shared_ptr<Texture2D> AssetLoader::CreateTextureFromRaw(
    int width, int height, int channels, const unsigned char* pixels)
{
    if (width <= 0 || height <= 0 || channels <= 0) return nullptr;

    uint32_t internalFormat = GL_RGBA8, dataFormat = GL_RGBA;
    if (channels == 4) { internalFormat = GL_RGBA8; dataFormat = GL_RGBA; }
    else if (channels == 3) { internalFormat = GL_RGB8; dataFormat = GL_RGB; }
    else if (channels == 2) { internalFormat = GL_RG8; dataFormat = GL_RG; }
    else if (channels == 1) { internalFormat = GL_R8; dataFormat = GL_RED; }

    int mipLevels = static_cast<int>(std::floor(std::log2(std::max(width, height)))) + 1;

    uint32_t texID = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &texID);
    glTextureStorage2D(texID, mipLevels, internalFormat, width, height);

    glTextureParameteri(texID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(texID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(texID, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(texID, GL_TEXTURE_WRAP_T, GL_REPEAT);

    float maxAniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    if (maxAniso > 0.0f)
        glTextureParameterf(texID, GL_TEXTURE_MAX_ANISOTROPY, std::min(maxAniso, 16.0f));

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(texID, 0, 0, 0, width, height, dataFormat, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glGenerateTextureMipmap(texID);

    // Create a blank Texture2D then overwrite its internals via friend access
    auto tex = std::make_shared<Texture2D>(width, height, nullptr);
    // Delete the GL texture created by the constructor
    if (tex->m_RendererID)
        glDeleteTextures(1, &tex->m_RendererID);

    // Patch private members (AssetLoader is a friend of Texture2D)
    tex->m_RendererID = texID;
    tex->m_Channels = channels;
    tex->m_InternalFormat = internalFormat;
    tex->m_DataFormat = dataFormat;

    return tex;
}

// ---------------------------------------------------------------------------
// Public API — Standalone Texture
// ---------------------------------------------------------------------------
std::shared_ptr<Texture2D> AssetLoader::LoadTexture(const std::string& passetPath) {
    std::ifstream file(passetPath, std::ios::binary);
    if (!file.is_open()) {
        PULUO_CORE_ERROR("AssetLoader: Cannot open '{}'", passetPath);
        return nullptr;
    }

    PAssetHeader header;
    if (!ReadVal(file, header)) return nullptr;
    if (header.magic != PASSET_MAGIC || header.version != PASSET_VERSION) {
        PULUO_CORE_ERROR("AssetLoader: Invalid .passet header in '{}'", passetPath);
        return nullptr;
    }
    if (header.type != static_cast<uint32_t>(PAssetType::Texture)) {
        PULUO_CORE_ERROR("AssetLoader: '{}' is not a Texture asset", passetPath);
        return nullptr;
    }

    PAssetTextureEntry entry;
    if (!ReadVal(file, entry)) return nullptr;

    std::vector<unsigned char> pixels(entry.dataSize);
    if (entry.dataSize > 0) {
        if (!ReadBytes(file, pixels.data(), entry.dataSize)) return nullptr;
    }

    auto tex = CreateTextureFromRaw(
        static_cast<int>(entry.width),
        static_cast<int>(entry.height),
        static_cast<int>(entry.channels),
        pixels.data());

    if (tex) {
        PULUO_CORE_INFO("AssetLoader: Loaded texture '{}' ({}x{}, {} ch)",
                        passetPath, entry.width, entry.height, entry.channels);
    }
    return tex;
}

// ---------------------------------------------------------------------------
// Public API — Model
// ---------------------------------------------------------------------------
std::shared_ptr<Model> AssetLoader::LoadModel(const std::string& passetPath) {
    std::ifstream file(passetPath, std::ios::binary);
    if (!file.is_open()) {
        PULUO_CORE_ERROR("AssetLoader: Cannot open '{}'", passetPath);
        return nullptr;
    }

    // Header
    PAssetHeader header;
    if (!ReadVal(file, header)) return nullptr;
    if (header.magic != PASSET_MAGIC || header.version != PASSET_VERSION) {
        PULUO_CORE_ERROR("AssetLoader: Invalid .passet header in '{}'", passetPath);
        return nullptr;
    }
    if (header.type != static_cast<uint32_t>(PAssetType::Model)) {
        PULUO_CORE_ERROR("AssetLoader: '{}' is not a Model asset", passetPath);
        return nullptr;
    }

    // Counts
    uint32_t texCount, matCount, meshCount;
    ReadVal(file, texCount);
    ReadVal(file, matCount);
    ReadVal(file, meshCount);

    // Model AABB
    float aabb[6];
    ReadBytes(file, aabb, sizeof(aabb));

    // ---- Read textures ----
    std::vector<std::shared_ptr<Texture2D>> textures(texCount);
    for (uint32_t i = 0; i < texCount; i++) {
        PAssetTextureEntry entry;
        if (!ReadVal(file, entry)) return nullptr;

        std::vector<unsigned char> pixels(entry.dataSize);
        if (entry.dataSize > 0) {
            if (!ReadBytes(file, pixels.data(), entry.dataSize)) return nullptr;
        }

        textures[i] = CreateTextureFromRaw(
            static_cast<int>(entry.width),
            static_cast<int>(entry.height),
            static_cast<int>(entry.channels),
            pixels.data());
    }

    // ---- Read materials ----
    std::vector<PBRMaterialData> materials(matCount);
    for (uint32_t i = 0; i < matCount; i++) {
        PAssetMaterial pm;
        if (!ReadVal(file, pm)) return nullptr;

        PBRMaterialData& mat = materials[i];
        mat.albedo = {pm.albedo[0], pm.albedo[1], pm.albedo[2]};
        mat.metallic = pm.metallic;
        mat.roughness = pm.roughness;
        mat.ao = pm.ao;

        auto getTexOrNull = [&](int32_t idx) -> std::shared_ptr<Texture2D> {
            if (idx >= 0 && idx < static_cast<int32_t>(textures.size()))
                return textures[idx];
            return nullptr;
        };
        mat.albedoMap    = getTexOrNull(pm.albedoMapIndex);
        mat.normalMap    = getTexOrNull(pm.normalMapIndex);
        mat.metallicMap  = getTexOrNull(pm.metallicMapIndex);
        mat.roughnessMap = getTexOrNull(pm.roughnessMapIndex);
        mat.aoMap        = getTexOrNull(pm.aoMapIndex);
        mat.maskMap      = getTexOrNull(pm.maskMapIndex);

        mat.alphaCutoff = pm.alphaCutoff;
        mat.useAlphaMask = pm.useAlphaMask != 0;
        mat.useSSS = pm.useSSS != 0;
        mat.sssColor = {pm.sssColor[0], pm.sssColor[1], pm.sssColor[2]};
        mat.sssStrength = pm.sssStrength;
    }

    // ---- Read meshes ----
    std::vector<ModelMesh> modelMeshes;
    modelMeshes.reserve(meshCount);
    for (uint32_t i = 0; i < meshCount; i++) {
        PAssetMeshHeader mh;
        if (!ReadVal(file, mh)) return nullptr;

        std::vector<Vertex> vertices(mh.vertexCount);
        if (mh.vertexCount > 0) {
            if (!ReadBytes(file, vertices.data(), mh.vertexCount * sizeof(Vertex)))
                return nullptr;
        }

        std::vector<uint32_t> indices(mh.indexCount);
        if (mh.indexCount > 0) {
            if (!ReadBytes(file, indices.data(), mh.indexCount * sizeof(uint32_t)))
                return nullptr;
        }

        ModelMesh mm;
        mm.mesh = Mesh(vertices, indices);
        mm.materialIndex = mh.materialIndex;
        mm.localAABB.min = {mh.aabbMin[0], mh.aabbMin[1], mh.aabbMin[2]};
        mm.localAABB.max = {mh.aabbMax[0], mh.aabbMax[1], mh.aabbMax[2]};
        modelMeshes.push_back(std::move(mm));
    }

    // ---- Assemble Model (friend access) ----
    auto model = std::make_shared<Model>();
    model->m_Meshes = std::move(modelMeshes);
    model->m_Materials = std::move(materials);
    model->m_BoundingBox.min = {aabb[0], aabb[1], aabb[2]};
    model->m_BoundingBox.max = {aabb[3], aabb[4], aabb[5]};

    PULUO_CORE_INFO("AssetLoader: Loaded '{}' ({} textures, {} materials, {} meshes)",
                    passetPath, texCount, matCount, meshCount);
    return model;
}

} // namespace Puluo
