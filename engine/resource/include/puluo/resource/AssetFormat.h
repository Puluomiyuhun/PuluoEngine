#pragma once

#include <cstdint>

namespace Puluo {

// .passet binary format constants
constexpr uint32_t PASSET_MAGIC   = 0x4F554C50; // "PLUO" in little-endian
constexpr uint32_t PASSET_VERSION = 1;

enum class PAssetType : uint32_t {
    Texture = 0,
    Model   = 1,
};

// File header (12 bytes)
struct PAssetHeader {
    uint32_t magic   = PASSET_MAGIC;
    uint32_t version = PASSET_VERSION;
    uint32_t type    = 0;
};

// Inline texture blob within a Model asset
struct PAssetTextureEntry {
    uint32_t width    = 0;
    uint32_t height   = 0;
    uint32_t channels = 0;
    uint32_t dataSize = 0; // width * height * channels
    // Followed by dataSize bytes of raw pixel data
};

// PBR material parameters (fixed-size, no pointers)
struct PAssetMaterial {
    float albedo[3]    = {1.0f, 1.0f, 1.0f};
    float metallic     = 0.0f;
    float roughness    = 0.5f;
    float ao           = 1.0f;

    int32_t albedoMapIndex    = -1; // index into texture array, -1 = none
    int32_t normalMapIndex    = -1;
    int32_t metallicMapIndex  = -1;
    int32_t roughnessMapIndex = -1;
    int32_t aoMapIndex        = -1;
    int32_t maskMapIndex      = -1;

    float alphaCutoff   = 0.5f;
    uint8_t useAlphaMask = 0;
    uint8_t useSSS       = 0;
    uint8_t _pad[2]      = {};

    float sssColor[3]    = {1.0f, 1.0f, 1.0f};
    float sssStrength    = 0.5f;
};

// Mesh header (followed by vertex data then index data)
struct PAssetMeshHeader {
    int32_t  materialIndex = -1;
    uint32_t vertexCount   = 0;
    uint32_t indexCount     = 0;
    float    aabbMin[3]    = {};
    float    aabbMax[3]    = {};
    // Followed by vertexCount * sizeof(Vertex) bytes
    // Followed by indexCount * sizeof(uint32_t) bytes
};

// Model asset layout (after PAssetHeader):
//   uint32_t textureCount
//   uint32_t materialCount
//   uint32_t meshCount
//   float    modelAABBMin[3]
//   float    modelAABBMax[3]
//   PAssetTextureEntry[textureCount] (each followed by pixel data)
//   PAssetMaterial[materialCount]
//   PAssetMeshHeader[meshCount] (each followed by vertex + index data)

} // namespace Puluo
