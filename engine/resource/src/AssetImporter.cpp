#include "puluo/resource/AssetImporter.h"
#include "puluo/resource/AssetFormat.h"
#include "puluo/renderer/Mesh.h" // for Vertex layout
#include "puluo/core/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "stb/stb_image.h"

#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

namespace Puluo {

// ---------------------------------------------------------------------------
// Helpers (mirror Model.cpp logic for texture path resolution)
// ---------------------------------------------------------------------------
static std::string GetTextureRefImporter(aiMaterial* mat, aiTextureType type) {
    if (mat->GetTextureCount(type) > 0) {
        aiString path;
        mat->GetTexture(type, 0, &path);
        return path.C_Str();
    }
    return "";
}

static std::string ResolveTexturePathImporter(const std::string& ref, const std::string& directory) {
    std::string fullPath = directory + "/" + ref;
    if (std::filesystem::exists(fullPath))
        return fullPath;

    std::string filename = std::filesystem::path(ref).filename().string();
    std::string fallbackPath = directory + "/" + filename;
    if (std::filesystem::exists(fallbackPath))
        return fallbackPath;

    std::string texturesPath = directory + "/textures/" + filename;
    if (std::filesystem::exists(texturesPath))
        return texturesPath;

    return fullPath;
}

// ---------------------------------------------------------------------------
// Decoded texture data (CPU-side)
// ---------------------------------------------------------------------------
struct DecodedTexture {
    int width = 0, height = 0, channels = 0;
    std::vector<unsigned char> pixels;
};

// ---------------------------------------------------------------------------
// Decode a texture reference (embedded or external) into raw pixels
// ---------------------------------------------------------------------------
static bool DecodeTexture(const std::string& ref, const aiScene* scene,
                          const std::string& modelDir, DecodedTexture& out) {
    stbi_set_flip_vertically_on_load(1);

    // Embedded texture
    if (!ref.empty() && ref[0] == '*') {
        int idx = std::atoi(ref.c_str() + 1);
        if (idx < 0 || static_cast<unsigned>(idx) >= scene->mNumTextures) return false;
        const aiTexture* aiTex = scene->mTextures[idx];
        if (!aiTex || !aiTex->pcData) return false;

        if (aiTex->mHeight == 0) {
            // Compressed (PNG/JPG in memory)
            unsigned char* px = stbi_load_from_memory(
                reinterpret_cast<const unsigned char*>(aiTex->pcData),
                static_cast<int>(aiTex->mWidth),
                &out.width, &out.height, &out.channels, 0);
            if (!px) return false;
            size_t sz = static_cast<size_t>(out.width) * out.height * out.channels;
            out.pixels.assign(px, px + sz);
            stbi_image_free(px);
            return true;
        } else {
            // Uncompressed ARGB8888
            out.width = static_cast<int>(aiTex->mWidth);
            out.height = static_cast<int>(aiTex->mHeight);
            out.channels = 4;
            size_t sz = static_cast<size_t>(out.width) * out.height * 4;
            out.pixels.resize(sz);
            std::memcpy(out.pixels.data(), aiTex->pcData, sz);
            return true;
        }
    }

    // External file
    std::string path = ResolveTexturePathImporter(ref, modelDir);
    int w, h, ch;
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (!px) return false;
    out.width = w;
    out.height = h;
    out.channels = ch;
    size_t sz = static_cast<size_t>(w) * h * ch;
    out.pixels.assign(px, px + sz);
    stbi_image_free(px);
    return true;
}

// ---------------------------------------------------------------------------
// Downscale a decoded texture to fit within maxSize (box filter)
// ---------------------------------------------------------------------------
static void DownscaleTexture(DecodedTexture& tex, int maxSize) {
    if (tex.width <= maxSize && tex.height <= maxSize) return;

    // Compute new size preserving aspect ratio
    int newW = tex.width, newH = tex.height;
    if (newW > newH) {
        newH = newH * maxSize / newW;
        newW = maxSize;
    } else {
        newW = newW * maxSize / newH;
        newH = maxSize;
    }
    newW = std::max(newW, 1);
    newH = std::max(newH, 1);

    int ch = tex.channels;
    std::vector<unsigned char> dst(static_cast<size_t>(newW) * newH * ch);

    for (int dy = 0; dy < newH; dy++) {
        for (int dx = 0; dx < newW; dx++) {
            // Source region
            float sx0 = static_cast<float>(dx) * tex.width / newW;
            float sy0 = static_cast<float>(dy) * tex.height / newH;
            float sx1 = static_cast<float>(dx + 1) * tex.width / newW;
            float sy1 = static_cast<float>(dy + 1) * tex.height / newH;

            int ix0 = static_cast<int>(sx0);
            int iy0 = static_cast<int>(sy0);
            int ix1 = std::min(static_cast<int>(std::ceil(sx1)), tex.width);
            int iy1 = std::min(static_cast<int>(std::ceil(sy1)), tex.height);

            // Box filter average
            for (int c = 0; c < ch; c++) {
                float sum = 0.0f;
                int count = 0;
                for (int sy = iy0; sy < iy1; sy++) {
                    for (int sx = ix0; sx < ix1; sx++) {
                        sum += tex.pixels[(sy * tex.width + sx) * ch + c];
                        count++;
                    }
                }
                dst[(dy * newW + dx) * ch + c] = static_cast<unsigned char>(
                    std::clamp(sum / std::max(count, 1) + 0.5f, 0.0f, 255.0f));
            }
        }
    }

    PULUO_CORE_INFO("AssetImporter: Downscaled texture {}x{} -> {}x{}", tex.width, tex.height, newW, newH);
    tex.width = newW;
    tex.height = newH;
    tex.pixels = std::move(dst);
}

// ---------------------------------------------------------------------------
// Node tree helpers (for split import — no PreTransformVertices)
// ---------------------------------------------------------------------------
static aiMatrix4x4 GetWorldTransform(const aiNode* node) {
    if (node->mParent)
        return GetWorldTransform(node->mParent) * node->mTransformation;
    return node->mTransformation;
}

static void CollectMeshesRecursive(const aiNode* node, std::vector<unsigned int>& out) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
        out.push_back(node->mMeshes[i]);
    for (unsigned int i = 0; i < node->mNumChildren; i++)
        CollectMeshesRecursive(node->mChildren[i], out);
}

static Vec3 TransformPoint(const aiMatrix4x4& m, float x, float y, float z) {
    return {
        m.a1*x + m.a2*y + m.a3*z + m.a4,
        m.b1*x + m.b2*y + m.b3*z + m.b4,
        m.c1*x + m.c2*y + m.c3*z + m.c4
    };
}

static Vec3 TransformDir(const aiMatrix4x4& m, float x, float y, float z) {
    Vec3 r = {
        m.a1*x + m.a2*y + m.a3*z,
        m.b1*x + m.b2*y + m.b3*z,
        m.c1*x + m.c2*y + m.c3*z
    };
    float len = std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z);
    if (len > 1e-6f) { r.x /= len; r.y /= len; r.z /= len; }
    return r;
}

// Sanitize a node name into a valid filename component
static std::string SanitizeNodeName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|')
            out += '_';
        else
            out += c;
    }
    if (out.empty()) out = "unnamed";
    return out;
}

// ---------------------------------------------------------------------------
// Helper to write raw bytes
// ---------------------------------------------------------------------------
template<typename T>
static void WriteVal(std::ofstream& f, const T& val) {
    f.write(reinterpret_cast<const char*>(&val), sizeof(T));
}

static void WriteBytes(std::ofstream& f, const void* data, size_t size) {
    f.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
}

// Write a .passet for a subset of meshes from a scene, applying a world transform
static std::string WritePAssetForMeshSubset(
    const aiScene* scene,
    const std::string& modelDir,
    const std::vector<unsigned int>& meshIndices,
    const aiMatrix4x4& worldTransform,
    const std::string& outPath)
{
    if (meshIndices.empty()) return "";

    // ---- Collect used materials ----
    std::unordered_set<unsigned int> usedMatSet;
    for (unsigned int mi : meshIndices) {
        if (mi < scene->mNumMeshes)
            usedMatSet.insert(scene->mMeshes[mi]->mMaterialIndex);
    }
    // Build old→new material index mapping
    std::vector<unsigned int> usedMatIndices(usedMatSet.begin(), usedMatSet.end());
    std::sort(usedMatIndices.begin(), usedMatIndices.end());
    std::unordered_map<unsigned int, int32_t> matRemap;
    for (size_t i = 0; i < usedMatIndices.size(); i++)
        matRemap[usedMatIndices[i]] = static_cast<int32_t>(i);

    // ---- Decode textures (only for used materials) ----
    std::vector<DecodedTexture> decodedTextures;
    std::unordered_map<std::string, int32_t> texRefToIndex;

    auto resolveTexIndex = [&](aiMaterial* mat, aiTextureType type) -> int32_t {
        std::string ref = GetTextureRefImporter(mat, type);
        if (ref.empty()) return -1;
        auto it = texRefToIndex.find(ref);
        if (it != texRefToIndex.end()) return it->second;
        DecodedTexture decoded;
        if (!DecodeTexture(ref, scene, modelDir, decoded)) {
            texRefToIndex[ref] = -1;
            return -1;
        }
        DownscaleTexture(decoded, 4096);
        int32_t idx = static_cast<int32_t>(decodedTextures.size());
        decodedTextures.push_back(std::move(decoded));
        texRefToIndex[ref] = idx;
        return idx;
    };

    // ---- Build materials ----
    std::vector<PAssetMaterial> materials;
    for (unsigned int oldIdx : usedMatIndices) {
        aiMaterial* mat = scene->mMaterials[oldIdx];
        PAssetMaterial pm{};
        aiColor3D color(1.0f, 1.0f, 1.0f);
        mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        pm.albedo[0] = color.r; pm.albedo[1] = color.g; pm.albedo[2] = color.b;
        float metallic = 0.0f, roughness = 0.5f;
        mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
        mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        pm.metallic = metallic;
        pm.roughness = roughness;
        pm.albedoMapIndex = resolveTexIndex(mat, aiTextureType_DIFFUSE);
        if (pm.albedoMapIndex < 0)
            pm.albedoMapIndex = resolveTexIndex(mat, aiTextureType_BASE_COLOR);
        pm.normalMapIndex = resolveTexIndex(mat, aiTextureType_NORMALS);
        if (pm.normalMapIndex < 0)
            pm.normalMapIndex = resolveTexIndex(mat, aiTextureType_HEIGHT);
        pm.metallicMapIndex = resolveTexIndex(mat, aiTextureType_METALNESS);
        pm.roughnessMapIndex = resolveTexIndex(mat, aiTextureType_DIFFUSE_ROUGHNESS);
        pm.aoMapIndex = resolveTexIndex(mat, aiTextureType_AMBIENT_OCCLUSION);
        if (pm.aoMapIndex < 0)
            pm.aoMapIndex = resolveTexIndex(mat, aiTextureType_LIGHTMAP);
        pm.maskMapIndex = resolveTexIndex(mat, aiTextureType_OPACITY);
        if (pm.maskMapIndex >= 0) { pm.useAlphaMask = 1; pm.useSSS = 1; }
        materials.push_back(pm);
    }

    // ---- Build meshes with manual transform ----
    struct MeshData {
        PAssetMeshHeader header;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };
    std::vector<MeshData> meshes;
    AABB modelAABB;

    for (unsigned int mi : meshIndices) {
        if (mi >= scene->mNumMeshes) continue;
        aiMesh* mesh = scene->mMeshes[mi];
        if (mesh->mNumVertices == 0 || mesh->mNumFaces == 0) continue;

        MeshData md;
        md.header.materialIndex = matRemap[mesh->mMaterialIndex];
        md.header.vertexCount = mesh->mNumVertices;

        AABB meshAABB;
        md.vertices.reserve(mesh->mNumVertices);
        for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
            Vertex vert{};
            vert.position = TransformPoint(worldTransform,
                mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
            meshAABB.Expand(vert.position);
            if (mesh->HasNormals())
                vert.normal = TransformDir(worldTransform,
                    mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
            if (mesh->mTextureCoords[0])
                vert.texCoord = {mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y};
            if (mesh->HasTangentsAndBitangents())
                vert.tangent = TransformDir(worldTransform,
                    mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z);
            md.vertices.push_back(vert);
        }

        for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
            aiFace& face = mesh->mFaces[f];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                md.indices.push_back(face.mIndices[j]);
        }

        md.header.indexCount = static_cast<uint32_t>(md.indices.size());
        modelAABB.Merge(meshAABB);
        meshes.push_back(std::move(md));
    }

    if (meshes.empty()) return "";

    // ---- Re-center all vertices to AABB center (fix pivot point) ----
    Vec3 center = {
        (modelAABB.min.x + modelAABB.max.x) * 0.5f,
        (modelAABB.min.y + modelAABB.max.y) * 0.5f,
        (modelAABB.min.z + modelAABB.max.z) * 0.5f
    };
    AABB recenteredAABB;
    for (auto& md : meshes) {
        AABB meshAABB;
        for (auto& vert : md.vertices) {
            vert.position.x -= center.x;
            vert.position.y -= center.y;
            vert.position.z -= center.z;
            meshAABB.Expand(vert.position);
        }
        md.header.aabbMin[0] = meshAABB.min.x; md.header.aabbMin[1] = meshAABB.min.y; md.header.aabbMin[2] = meshAABB.min.z;
        md.header.aabbMax[0] = meshAABB.max.x; md.header.aabbMax[1] = meshAABB.max.y; md.header.aabbMax[2] = meshAABB.max.z;
        recenteredAABB.Merge(meshAABB);
    }
    modelAABB = recenteredAABB;

    // ---- Write .passet ----
    std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
    std::ofstream file(outPath, std::ios::binary);
    if (!file.is_open()) {
        PULUO_CORE_ERROR("AssetImporter: Cannot write '{}'", outPath);
        return "";
    }

    PAssetHeader header;
    header.type = static_cast<uint32_t>(PAssetType::Model);
    WriteVal(file, header);

    uint32_t texCount = static_cast<uint32_t>(decodedTextures.size());
    uint32_t matCount = static_cast<uint32_t>(materials.size());
    uint32_t meshCount = static_cast<uint32_t>(meshes.size());
    WriteVal(file, texCount);
    WriteVal(file, matCount);
    WriteVal(file, meshCount);

    float aabb[6] = {modelAABB.min.x, modelAABB.min.y, modelAABB.min.z,
                     modelAABB.max.x, modelAABB.max.y, modelAABB.max.z};
    WriteBytes(file, aabb, sizeof(aabb));

    for (auto& tex : decodedTextures) {
        PAssetTextureEntry entry;
        entry.width = static_cast<uint32_t>(tex.width);
        entry.height = static_cast<uint32_t>(tex.height);
        entry.channels = static_cast<uint32_t>(tex.channels);
        entry.dataSize = static_cast<uint32_t>(tex.pixels.size());
        WriteVal(file, entry);
        WriteBytes(file, tex.pixels.data(), tex.pixels.size());
    }
    for (auto& mat : materials) WriteVal(file, mat);
    for (auto& md : meshes) {
        WriteVal(file, md.header);
        WriteBytes(file, md.vertices.data(), md.vertices.size() * sizeof(Vertex));
        WriteBytes(file, md.indices.data(), md.indices.size() * sizeof(uint32_t));
    }

    file.close();
    PULUO_CORE_INFO("AssetImporter: Split export -> '{}' ({} tex, {} mat, {} mesh)",
                    outPath, texCount, matCount, meshCount);
    return outPath;     
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
std::string AssetImporter::GetCachePath(const std::string& sourcePath, const std::string& cacheDir) {
    // "assets/models/model.glb" -> "assets/cache/models/model.glb.passet"
    std::filesystem::path src(sourcePath);
    std::string relative = src.string();
    // Normalize
    std::replace(relative.begin(), relative.end(), '\\', '/');

    // Strip leading "assets/" if present for cleaner cache paths
    std::string prefix = "assets/";
    std::string subpath = relative;
    if (subpath.substr(0, prefix.size()) == prefix)
        subpath = subpath.substr(prefix.size());

    return cacheDir + "/" + subpath + ".passet";
}

// ---------------------------------------------------------------------------
// Shared implementation: texture
// ---------------------------------------------------------------------------
std::string AssetImporter::WriteTexturePAsset(const std::string& sourcePath, const std::string& outPath) {
    stbi_set_flip_vertically_on_load(1);

    int w, h, ch;
    unsigned char* px = stbi_load(sourcePath.c_str(), &w, &h, &ch, 0);
    if (!px) {
        PULUO_CORE_ERROR("AssetImporter: stbi_load failed for '{}'", sourcePath);
        return "";
    }

    size_t pixelSize = static_cast<size_t>(w) * h * ch;

    std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());

    std::ofstream file(outPath, std::ios::binary);
    if (!file.is_open()) {
        stbi_image_free(px);
        PULUO_CORE_ERROR("AssetImporter: Cannot write '{}'", outPath);
        return "";
    }

    PAssetHeader header;
    header.type = static_cast<uint32_t>(PAssetType::Texture);
    WriteVal(file, header);

    PAssetTextureEntry entry;
    entry.width = static_cast<uint32_t>(w);
    entry.height = static_cast<uint32_t>(h);
    entry.channels = static_cast<uint32_t>(ch);
    entry.dataSize = static_cast<uint32_t>(pixelSize);
    WriteVal(file, entry);
    WriteBytes(file, px, pixelSize);

    stbi_image_free(px);
    file.close();

    PULUO_CORE_INFO("AssetImporter: Exported texture '{}' -> '{}' ({}x{}, {} ch)",
                    sourcePath, outPath, w, h, ch);
    return outPath;
}

// ---------------------------------------------------------------------------
// Shared implementation: model
// ---------------------------------------------------------------------------
std::string AssetImporter::WriteModelPAsset(const std::string& sourcePath, const std::string& outPath) {
    // Parse model with Assimp
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(sourcePath,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_PreTransformVertices);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        PULUO_CORE_ERROR("AssetImporter: Assimp error for '{}': {}", sourcePath, importer.GetErrorString());
        return "";
    }

    std::string modelDir = std::filesystem::path(sourcePath).parent_path().string();

    // ---- Decode all unique textures ----
    std::vector<DecodedTexture> decodedTextures;
    std::unordered_map<std::string, int32_t> texRefToIndex;

    auto resolveTexIndex = [&](aiMaterial* mat, aiTextureType type) -> int32_t {
        std::string ref = GetTextureRefImporter(mat, type);
        if (ref.empty()) return -1;

        auto it = texRefToIndex.find(ref);
        if (it != texRefToIndex.end()) return it->second;

        DecodedTexture decoded;
        if (!DecodeTexture(ref, scene, modelDir, decoded)) {
            PULUO_CORE_WARN("AssetImporter: Failed to decode texture '{}'", ref);
            texRefToIndex[ref] = -1;
            return -1;
        }

        // Limit model textures to 4096x4096 to reduce .passet file size
        DownscaleTexture(decoded, 4096);

        int32_t idx = static_cast<int32_t>(decodedTextures.size());
        decodedTextures.push_back(std::move(decoded));
        texRefToIndex[ref] = idx;
        return idx;
    };

    // ---- Build materials ----
    std::vector<PAssetMaterial> materials;
    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* mat = scene->mMaterials[i];
        PAssetMaterial pm{};

        aiColor3D color(1.0f, 1.0f, 1.0f);
        mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        pm.albedo[0] = color.r; pm.albedo[1] = color.g; pm.albedo[2] = color.b;

        float metallic = 0.0f, roughness = 0.5f;
        mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
        mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        pm.metallic = metallic;
        pm.roughness = roughness;

        pm.albedoMapIndex = resolveTexIndex(mat, aiTextureType_DIFFUSE);
        if (pm.albedoMapIndex < 0)
            pm.albedoMapIndex = resolveTexIndex(mat, aiTextureType_BASE_COLOR);

        pm.normalMapIndex = resolveTexIndex(mat, aiTextureType_NORMALS);
        if (pm.normalMapIndex < 0)
            pm.normalMapIndex = resolveTexIndex(mat, aiTextureType_HEIGHT);

        pm.metallicMapIndex = resolveTexIndex(mat, aiTextureType_METALNESS);
        pm.roughnessMapIndex = resolveTexIndex(mat, aiTextureType_DIFFUSE_ROUGHNESS);

        pm.aoMapIndex = resolveTexIndex(mat, aiTextureType_AMBIENT_OCCLUSION);
        if (pm.aoMapIndex < 0)
            pm.aoMapIndex = resolveTexIndex(mat, aiTextureType_LIGHTMAP);

        pm.maskMapIndex = resolveTexIndex(mat, aiTextureType_OPACITY);
        if (pm.maskMapIndex >= 0) {
            pm.useAlphaMask = 1;
            pm.useSSS = 1;
        }

        materials.push_back(pm);
    }

    // ---- Build meshes ----
    struct MeshData {
        PAssetMeshHeader header;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };
    std::vector<MeshData> meshes;

    AABB modelAABB;

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];
        if (mesh->mNumVertices == 0 || mesh->mNumFaces == 0) continue;

        MeshData md;
        md.header.materialIndex = static_cast<int32_t>(mesh->mMaterialIndex);
        md.header.vertexCount = mesh->mNumVertices;

        AABB meshAABB;
        md.vertices.reserve(mesh->mNumVertices);
        for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
            Vertex vert{};
            vert.position = {mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z};
            meshAABB.Expand(vert.position);
            if (mesh->HasNormals())
                vert.normal = {mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z};
            if (mesh->mTextureCoords[0])
                vert.texCoord = {mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y};
            if (mesh->HasTangentsAndBitangents())
                vert.tangent = {mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z};
            md.vertices.push_back(vert);
        }

        for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
            aiFace& face = mesh->mFaces[f];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                md.indices.push_back(face.mIndices[j]);
        }

        md.header.indexCount = static_cast<uint32_t>(md.indices.size());
        md.header.aabbMin[0] = meshAABB.min.x;
        md.header.aabbMin[1] = meshAABB.min.y;
        md.header.aabbMin[2] = meshAABB.min.z;
        md.header.aabbMax[0] = meshAABB.max.x;
        md.header.aabbMax[1] = meshAABB.max.y;
        md.header.aabbMax[2] = meshAABB.max.z;

        modelAABB.Merge(meshAABB);
        meshes.push_back(std::move(md));
    }

    // ---- Write .passet file ----
    std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());

    std::ofstream file(outPath, std::ios::binary);
    if (!file.is_open()) {
        PULUO_CORE_ERROR("AssetImporter: Cannot write '{}'", outPath);
        return "";
    }

    // Header
    PAssetHeader header;
    header.type = static_cast<uint32_t>(PAssetType::Model);
    WriteVal(file, header);

    // Counts
    uint32_t texCount = static_cast<uint32_t>(decodedTextures.size());
    uint32_t matCount = static_cast<uint32_t>(materials.size());
    uint32_t meshCount = static_cast<uint32_t>(meshes.size());
    WriteVal(file, texCount);
    WriteVal(file, matCount);
    WriteVal(file, meshCount);

    // Model AABB
    float aabb[6] = {modelAABB.min.x, modelAABB.min.y, modelAABB.min.z,
                     modelAABB.max.x, modelAABB.max.y, modelAABB.max.z};
    WriteBytes(file, aabb, sizeof(aabb));

    // Textures
    for (auto& tex : decodedTextures) {
        PAssetTextureEntry entry;
        entry.width = static_cast<uint32_t>(tex.width);
        entry.height = static_cast<uint32_t>(tex.height);
        entry.channels = static_cast<uint32_t>(tex.channels);
        entry.dataSize = static_cast<uint32_t>(tex.pixels.size());
        WriteVal(file, entry);
        WriteBytes(file, tex.pixels.data(), tex.pixels.size());
    }

    // Materials
    for (auto& mat : materials) {
        WriteVal(file, mat);
    }

    // Meshes
    for (auto& md : meshes) {
        WriteVal(file, md.header);
        WriteBytes(file, md.vertices.data(), md.vertices.size() * sizeof(Vertex));
        WriteBytes(file, md.indices.data(), md.indices.size() * sizeof(uint32_t));
    }

    file.close();

    PULUO_CORE_INFO("AssetImporter: Exported '{}' -> '{}' ({} textures, {} materials, {} meshes)",
                    sourcePath, outPath, texCount, matCount, meshCount);
    return outPath;
}

// ---------------------------------------------------------------------------
// Import-to-project API
// ---------------------------------------------------------------------------
std::string AssetImporter::ImportModelToProject(const std::string& externalPath, const std::string& destDir) {
    std::string filename = std::filesystem::path(externalPath).stem().string() + ".passet";
    std::string outPath = destDir + "/" + filename;

    // Handle name collision: soldier.passet → soldier_1.passet
    if (std::filesystem::exists(outPath)) {
        int counter = 1;
        std::string stem = std::filesystem::path(externalPath).stem().string();
        do {
            outPath = destDir + "/" + stem + "_" + std::to_string(counter) + ".passet";
            ++counter;
        } while (std::filesystem::exists(outPath));
    }

    return WriteModelPAsset(externalPath, outPath);
}

std::string AssetImporter::ImportTextureToProject(const std::string& externalPath, const std::string& destDir) {
    std::string filename = std::filesystem::path(externalPath).stem().string() + ".passet";
    std::string outPath = destDir + "/" + filename;

    // Handle name collision
    if (std::filesystem::exists(outPath)) {
        int counter = 1;
        std::string stem = std::filesystem::path(externalPath).stem().string();
        do {
            outPath = destDir + "/" + stem + "_" + std::to_string(counter) + ".passet";
            ++counter;
        } while (std::filesystem::exists(outPath));
    }

    return WriteTexturePAsset(externalPath, outPath);
}

// ---------------------------------------------------------------------------
// Split import: one FBX/GLB → N .passet files (one per top-level child node)
// ---------------------------------------------------------------------------
std::vector<std::string> AssetImporter::ImportModelSplitToProject(
    const std::string& externalPath, const std::string& destDir)
{
    std::vector<std::string> results;

    // Load WITHOUT PreTransformVertices to preserve node hierarchy
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(externalPath,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        PULUO_CORE_ERROR("AssetImporter: Assimp error for '{}': {}", externalPath, importer.GetErrorString());
        return results;
    }

    std::string modelDir = std::filesystem::path(externalPath).parent_path().string();
    std::string fileStem = std::filesystem::path(externalPath).stem().string();
    const aiNode* root = scene->mRootNode;

    // If root has no children but has meshes, treat as single object
    if (root->mNumChildren == 0) {
        std::vector<unsigned int> meshIndices;
        CollectMeshesRecursive(root, meshIndices);
        if (!meshIndices.empty()) {
            std::string outPath = destDir + "/" + fileStem + ".passet";
            if (std::filesystem::exists(outPath)) {
                int c = 1;
                do { outPath = destDir + "/" + fileStem + "_" + std::to_string(c++) + ".passet"; }
                while (std::filesystem::exists(outPath));
            }
            std::string r = WritePAssetForMeshSubset(scene, modelDir, meshIndices, root->mTransformation, outPath);
            if (!r.empty()) results.push_back(r);
        }
        return results;
    }

    // For each top-level child node, export as a separate .passet
    for (unsigned int i = 0; i < root->mNumChildren; i++) {
        const aiNode* child = root->mChildren[i];
        std::vector<unsigned int> meshIndices;
        CollectMeshesRecursive(child, meshIndices);
        if (meshIndices.empty()) continue;

        // Use child node name as filename, fallback to fileStem_index
        std::string nodeName = child->mName.length > 0
            ? SanitizeNodeName(child->mName.C_Str())
            : fileStem + "_" + std::to_string(i);

        std::string outPath = destDir + "/" + nodeName + ".passet";
        if (std::filesystem::exists(outPath)) {
            int c = 1;
            do { outPath = destDir + "/" + nodeName + "_" + std::to_string(c++) + ".passet"; }
            while (std::filesystem::exists(outPath));
        }

        // World transform = root transform * child transform (root has coord system conversion)
        aiMatrix4x4 worldTransform = GetWorldTransform(child);

        std::string r = WritePAssetForMeshSubset(scene, modelDir, meshIndices, worldTransform, outPath);
        if (!r.empty()) results.push_back(r);
    }

    PULUO_CORE_INFO("AssetImporter: Split import '{}' -> {} files", externalPath, results.size());
    return results;
}

// ---------------------------------------------------------------------------
// Legacy cache API (backward compatibility with old .pscene files)
// ---------------------------------------------------------------------------
std::string AssetImporter::Import(const std::string& sourcePath, const std::string& cacheDir) {
    return WriteModelPAsset(sourcePath, GetCachePath(sourcePath, cacheDir));
}

std::string AssetImporter::ImportTexture(const std::string& sourcePath, const std::string& cacheDir) {
    return WriteTexturePAsset(sourcePath, GetCachePath(sourcePath, cacheDir));
}

} // namespace Puluo
