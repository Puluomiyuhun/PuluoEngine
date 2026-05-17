#include "puluo/renderer/Model.h"
#include "puluo/resource/TextureCache.h"
#include "puluo/core/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>

namespace Puluo {

static std::string GetTextureRef(aiMaterial* mat, aiTextureType type) {
    if (mat->GetTextureCount(type) > 0) {
        aiString path;
        mat->GetTexture(type, 0, &path);
        return path.C_Str();
    }
    return "";
}

static std::string ResolveTexturePath(const std::string& ref, const std::string& directory) {
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

bool Model::Load(const std::string& filepath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        PULUO_CORE_ERROR("Assimp error: {0}", importer.GetErrorString());
        return false;
    }

    m_Directory = std::filesystem::path(filepath).parent_path().string();
    m_BoundingBox = AABB{}; // Reset bounding box

    // Load materials
    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* mat = scene->mMaterials[i];
        PBRMaterialData pbrMat;

        // Albedo color fallback
        aiColor3D color(1.0f, 1.0f, 1.0f);
        mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        pbrMat.albedo = Vec3(color.r, color.g, color.b);

        // PBR scalars
        float metallic = 0.0f, roughness = 0.5f;
        mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
        mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        pbrMat.metallic = metallic;
        pbrMat.roughness = roughness;

        // Textures - try loading with embedded texture support
        auto tryLoad = [&](aiTextureType type) -> std::shared_ptr<Texture2D> {
            std::string ref = GetTextureRef(mat, type);
            if (ref.empty()) return nullptr;
            return LoadTextureFromScene(ref, scene);
        };

        pbrMat.albedoMap = tryLoad(aiTextureType_DIFFUSE);
        if (!pbrMat.albedoMap)
            pbrMat.albedoMap = tryLoad(aiTextureType_BASE_COLOR);

        pbrMat.normalMap = tryLoad(aiTextureType_NORMALS);
        if (!pbrMat.normalMap)
            pbrMat.normalMap = tryLoad(aiTextureType_HEIGHT);

        pbrMat.metallicMap = tryLoad(aiTextureType_METALNESS);

        pbrMat.roughnessMap = tryLoad(aiTextureType_DIFFUSE_ROUGHNESS);

        pbrMat.aoMap = tryLoad(aiTextureType_AMBIENT_OCCLUSION);
        if (!pbrMat.aoMap)
            pbrMat.aoMap = tryLoad(aiTextureType_LIGHTMAP);

        // Opacity / alpha mask (for foliage cutout) — only explicit opacity texture
        pbrMat.maskMap = tryLoad(aiTextureType_OPACITY);
        if (pbrMat.maskMap) {
            pbrMat.useAlphaMask = true;
            // Auto-enable SSS for masked materials (likely foliage)
            pbrMat.useSSS = true;
        }

        m_Materials.push_back(std::move(pbrMat));
    }

    // Load meshes (flat traversal, ignore hierarchy for now)
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];

        // Skip empty or degenerate meshes
        if (mesh->mNumVertices == 0 || mesh->mNumFaces == 0)
            continue;

        std::vector<Vertex> vertices;
        vertices.reserve(mesh->mNumVertices);
           
        AABB meshAABB;
        for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
            Vertex vertex{};
            vertex.position = {mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z};
            meshAABB.Expand(vertex.position);

            if (mesh->HasNormals())
                vertex.normal = {mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z};

            if (mesh->mTextureCoords[0])
                vertex.texCoord = {mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y};

            if (mesh->HasTangentsAndBitangents())
                vertex.tangent = {mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z};

            vertices.push_back(vertex);
        }

        std::vector<uint32_t> indices;
        for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
            aiFace& face = mesh->mFaces[f];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        ModelMesh modelMesh;
        modelMesh.mesh = Mesh(vertices, indices);
        modelMesh.materialIndex = static_cast<int>(mesh->mMaterialIndex);
        modelMesh.localAABB = meshAABB;
        m_Meshes.push_back(std::move(modelMesh));

        m_BoundingBox.Merge(meshAABB);
    }

    PULUO_CORE_INFO("Model loaded: {0} ({1} meshes, {2} materials)", filepath, m_Meshes.size(), m_Materials.size());
    return true;
}

std::shared_ptr<Texture2D> Model::LoadTextureFromScene(const std::string& ref, const void* scenePtr) {
    const aiScene* scene = static_cast<const aiScene*>(scenePtr);

    try {
        // Check if this is an embedded texture (path starts with '*')
        if (!ref.empty() && ref[0] == '*') {
            int texIndex = std::atoi(ref.c_str() + 1);
            if (scene && texIndex >= 0 && static_cast<unsigned>(texIndex) < scene->mNumTextures) {
                const aiTexture* aiTex = scene->mTextures[texIndex];
                if (!aiTex || !aiTex->pcData) {
                    PULUO_CORE_WARN("Embedded texture {0} has null data", ref);
                    return nullptr;
                }
                // Cache key: "modelDirectory:*index"
                std::string cacheKey = m_Directory + ":" + ref;

                // Check cache first (covers both compressed and uncompressed)
                // LoadFromMemory only works for compressed formats (PNG/JPG)
                if (aiTex->mHeight == 0) {
                    if (aiTex->mWidth == 0) return nullptr;
                    // Compressed format (PNG/JPG stored as buffer)
                    return TextureCache::LoadFromMemory(
                        cacheKey,
                        reinterpret_cast<const unsigned char*>(aiTex->pcData),
                        aiTex->mWidth // mWidth = buffer size in bytes when mHeight == 0
                    );
                } else {
                    // Uncompressed ARGB8888 — rare case, not worth caching
                    auto tex = std::make_shared<Texture2D>(aiTex->mWidth, aiTex->mHeight);
                    tex->SetData(aiTex->pcData, aiTex->mWidth * aiTex->mHeight * 4);
                    return tex;
                }
            }
            PULUO_CORE_WARN("Embedded texture index out of range: {0}", ref);
            return nullptr;
        }

        // External file texture — go through cache
        std::string path = ResolveTexturePath(ref, m_Directory);
        return TextureCache::Load(path);
    } catch (...) {
        PULUO_CORE_WARN("Failed to load texture: {0}", ref);
        return nullptr;
    }
}

} // namespace Puluo
