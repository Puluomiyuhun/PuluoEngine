#include "puluo/renderer/Terrain.h"
#include "puluo/core/Log.h"

#include <glad/gl.h>
#include "stb/stb_image.h"
#include <cmath>
#include <algorithm>
#include <random>

namespace Puluo {

// Simple 2D hash for value noise
static float Hash2D(int x, int y, unsigned seed) {
    unsigned h = seed;
    h ^= static_cast<unsigned>(x) * 374761393u;
    h ^= static_cast<unsigned>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>(h & 0x7fffffff) / 2147483647.0f;
}

static float SmoothNoise2D(float x, float y, unsigned seed) {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float fx = x - ix;
    float fy = y - iy;

    // Smoothstep
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);

    float a = Hash2D(ix, iy, seed);
    float b = Hash2D(ix + 1, iy, seed);
    float c = Hash2D(ix, iy + 1, seed);
    float d = Hash2D(ix + 1, iy + 1, seed);

    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

static float FBM2D(float x, float y, int octaves, float frequency, unsigned seed) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float totalAmplitude = 0.0f;
    for (int i = 0; i < octaves; i++) {
        value += SmoothNoise2D(x * frequency, y * frequency, seed + i * 31) * amplitude;
        totalAmplitude += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return value / totalAmplitude;
}

Terrain::~Terrain() {
    Destroy();
}

void Terrain::Destroy() {
    if (m_HeightmapTex) {
        glDeleteTextures(1, &m_HeightmapTex);
        m_HeightmapTex = 0;
    }
    m_PatchVAO.reset();
    m_PatchVertexCount = 0;
    m_HeightData.clear();
    m_Created = false;
}

void Terrain::Create(const TerrainParams& params) {
    m_Params = params;
    m_HeightData.resize(params.heightmapRes * params.heightmapRes, 0.0f);

    GeneratePatchMesh();
    UploadHeightmap();
    m_Created = true;

    PULUO_CORE_INFO("Terrain created: {}x{} heightmap, {}x{} patches, {}m world size",
                     params.heightmapRes, params.heightmapRes,
                     params.patchCount, params.patchCount,
                     params.worldSize);
}

void Terrain::GenerateFromNoise(float frequency, int octaves) {
    if (!m_Created) return;

    int res = m_Params.heightmapRes;
    for (int y = 0; y < res; y++) {
        for (int x = 0; x < res; x++) {
            float nx = static_cast<float>(x) / static_cast<float>(res - 1);
            float ny = static_cast<float>(y) / static_cast<float>(res - 1);

            float h = FBM2D(nx, ny, octaves, frequency, 42);

            // Edge falloff to prevent cliffs at terrain borders
            float ex = 1.0f - std::pow(2.0f * nx - 1.0f, 4.0f);
            float ey = 1.0f - std::pow(2.0f * ny - 1.0f, 4.0f);
            h *= ex * ey;

            m_HeightData[y * res + x] = h;
        }
    }

    UploadHeightmap();
}

// Bilinear sample from a float buffer
static float SampleBilinear(const float* data, int w, int h, float u, float v) {
    float fx = u * (w - 1);
    float fy = v * (h - 1);
    int x0 = std::clamp(static_cast<int>(fx), 0, w - 2);
    int y0 = std::clamp(static_cast<int>(fy), 0, h - 2);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    float sx = fx - x0;
    float sy = fy - y0;
    float v00 = data[y0 * w + x0];
    float v10 = data[y0 * w + x1];
    float v01 = data[y1 * w + x0];
    float v11 = data[y1 * w + x1];
    return (v00 * (1 - sx) + v10 * sx) * (1 - sy) +
           (v01 * (1 - sx) + v11 * sx) * sy;
}

// Gaussian-like smooth pass to reduce 8-bit terracing
static void SmoothHeightData(std::vector<float>& data, int res, int iterations) {
    std::vector<float> tmp(data.size());
    for (int iter = 0; iter < iterations; iter++) {
        for (int y = 0; y < res; y++) {
            for (int x = 0; x < res; x++) {
                float sum = 0.0f;
                float weight = 0.0f;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = std::clamp(x + dx, 0, res - 1);
                        int ny = std::clamp(y + dy, 0, res - 1);
                        float w = (dx == 0 && dy == 0) ? 4.0f : (dx == 0 || dy == 0) ? 2.0f : 1.0f;
                        sum += data[ny * res + nx] * w;
                        weight += w;
                    }
                }
                tmp[y * res + x] = sum / weight;
            }
        }
        data.swap(tmp);
    }
}

bool Terrain::LoadFromImage(const std::string& imagePath) {
    if (!m_Created) return false;

    int imgW, imgH, channels;
    int res = m_Params.heightmapRes;
    bool is8bit = false;

    // Try 16-bit first for better precision
    stbi_us* data16 = stbi_load_16(imagePath.c_str(), &imgW, &imgH, &channels, 1);
    if (data16) {
        // Convert to float buffer for bilinear sampling
        std::vector<float> srcFloat(imgW * imgH);
        for (int i = 0; i < imgW * imgH; i++)
            srcFloat[i] = static_cast<float>(data16[i]) / 65535.0f;
        stbi_image_free(data16);

        for (int y = 0; y < res; y++) {
            for (int x = 0; x < res; x++) {
                float u = static_cast<float>(x) / static_cast<float>(res - 1);
                float v = static_cast<float>(y) / static_cast<float>(res - 1);
                m_HeightData[y * res + x] = SampleBilinear(srcFloat.data(), imgW, imgH, u, v);
            }
        }
        UploadHeightmap();
        PULUO_CORE_INFO("Terrain heightmap loaded (16-bit): {0} ({1}x{2})", imagePath, imgW, imgH);
        return true;
    }

    // Fallback: 8-bit
    unsigned char* data8 = stbi_load(imagePath.c_str(), &imgW, &imgH, &channels, 1);
    if (data8) {
        // Convert to float buffer for bilinear sampling
        std::vector<float> srcFloat(imgW * imgH);
        for (int i = 0; i < imgW * imgH; i++)
            srcFloat[i] = static_cast<float>(data8[i]) / 255.0f;
        stbi_image_free(data8);

        for (int y = 0; y < res; y++) {
            for (int x = 0; x < res; x++) {
                float u = static_cast<float>(x) / static_cast<float>(res - 1);
                float v = static_cast<float>(y) / static_cast<float>(res - 1);
                m_HeightData[y * res + x] = SampleBilinear(srcFloat.data(), imgW, imgH, u, v);
            }
        }
        // 8-bit only has 256 levels — smooth to reduce terracing
        SmoothHeightData(m_HeightData, res, 3);
        UploadHeightmap();
        PULUO_CORE_INFO("Terrain heightmap loaded (8-bit, smoothed): {0} ({1}x{2})", imagePath, imgW, imgH);
        return true;
    }

    PULUO_CORE_ERROR("Failed to load terrain heightmap: {0}", imagePath);
    return false;
}

float Terrain::GetHeightAt(float worldX, float worldZ) const {
    if (!m_Created) return 0.0f;

    float halfSize = m_Params.worldSize * 0.5f;
    float nx = (worldX + halfSize) / m_Params.worldSize;
    float nz = (worldZ + halfSize) / m_Params.worldSize;

    nx = std::clamp(nx, 0.0f, 1.0f);
    nz = std::clamp(nz, 0.0f, 1.0f);

    float fx = nx * (m_Params.heightmapRes - 1);
    float fy = nz * (m_Params.heightmapRes - 1);

    int ix = std::min(static_cast<int>(fx), m_Params.heightmapRes - 2);
    int iy = std::min(static_cast<int>(fy), m_Params.heightmapRes - 2);

    float u = fx - ix;
    float v = fy - iy;

    int res = m_Params.heightmapRes;
    float h00 = m_HeightData[iy * res + ix];
    float h10 = m_HeightData[iy * res + ix + 1];
    float h01 = m_HeightData[(iy + 1) * res + ix];
    float h11 = m_HeightData[(iy + 1) * res + ix + 1];

    float h = h00 * (1.0f - u) * (1.0f - v) + h10 * u * (1.0f - v) +
              h01 * (1.0f - u) * v + h11 * u * v;

    return h * m_Params.heightScale;
}

void Terrain::GeneratePatchMesh() {
    int pc = m_Params.patchCount;
    int vertCount = pc * pc * 4;
    std::vector<float> vertices(vertCount * 2); // vec2 per vertex

    int idx = 0;
    for (int py = 0; py < pc; py++) {
        for (int px = 0; px < pc; px++) {
            float x0 = static_cast<float>(px) / pc;
            float y0 = static_cast<float>(py) / pc;
            float x1 = static_cast<float>(px + 1) / pc;
            float y1 = static_cast<float>(py + 1) / pc;

            // BL, BR, TR, TL
            vertices[idx++] = x0; vertices[idx++] = y0;
            vertices[idx++] = x1; vertices[idx++] = y0;
            vertices[idx++] = x1; vertices[idx++] = y1;
            vertices[idx++] = x0; vertices[idx++] = y1;
        }
    }

    m_PatchVertexCount = vertCount;

    auto vbo = std::make_shared<VertexBuffer>(vertices.data(),
        static_cast<uint32_t>(vertices.size() * sizeof(float)));
    vbo->SetLayout({
        {"aPosition", ShaderDataType::Float2}
    });

    m_PatchVAO = std::make_shared<VertexArray>();
    m_PatchVAO->AddVertexBuffer(vbo);
}

void Terrain::UploadHeightmap() {
    int res = m_Params.heightmapRes;

    if (!m_HeightmapTex) {
        glGenTextures(1, &m_HeightmapTex);
        glBindTexture(GL_TEXTURE_2D, m_HeightmapTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, res, res, 0, GL_RED, GL_FLOAT, m_HeightData.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glBindTexture(GL_TEXTURE_2D, m_HeightmapTex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, res, res, GL_RED, GL_FLOAT, m_HeightData.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace Puluo
