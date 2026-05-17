#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>

namespace Puluo {

class NoiseGenerator {
public:
    // Generate a 3D Worley noise texture (RGBA: 3 octaves of Worley + 1 Perlin-Worley blend)
    // Returns RGBA8 data, dimensions: size^3 × 4 channels
    static std::vector<uint8_t> GenerateBaseNoise(int size, unsigned seed = 42) {
        std::vector<uint8_t> data(size * size * size * 4);

        // Higher cell counts for richer detail
        auto worley1 = Worley3D(size, 6, seed);       // low freq (was 4)
        auto worley2 = Worley3D(size, 12, seed + 1);  // mid freq (was 8)
        auto worley3 = Worley3D(size, 24, seed + 2);  // high freq (was 16)
        auto perlin  = Perlin3D(size, 6, seed + 3);   // Perlin (was 4)

        // FBM blend of Worley for richer structure
        for (int i = 0; i < size * size * size; i++) {
            float invW1 = 1.0f - worley1[i];
            float invW2 = 1.0f - worley2[i];
            float invW3 = 1.0f - worley3[i];
            // Perlin-Worley: Perlin modulated by Worley FBM for organic shapes
            float worleyFBM = invW1 * 0.625f + invW2 * 0.25f + invW3 * 0.125f;
            float pw = remap01(perlin[i], 0.0f, 1.0f) * 0.4f + worleyFBM * 0.6f;
            int idx = i * 4;
            data[idx + 0] = ToByte(pw);
            data[idx + 1] = ToByte(invW1);
            data[idx + 2] = ToByte(invW2);
            data[idx + 3] = ToByte(invW3);
        }

        return data;
    }

    // Generate a 3D detail Worley noise texture (RGB: 3 octaves, higher frequency)
    static std::vector<uint8_t> GenerateDetailNoise(int size, unsigned seed = 100) {
        std::vector<uint8_t> data(size * size * size * 4);

        // Higher frequency for finer edge detail
        auto worley1 = Worley3D(size, 4, seed);       // was 8 but at 32³ that's too dense
        auto worley2 = Worley3D(size, 8, seed + 1);   // was 16
        auto worley3 = Worley3D(size, 16, seed + 2);  // was 32

        for (int i = 0; i < size * size * size; i++) {
            int idx = i * 4;
            data[idx + 0] = ToByte(1.0f - worley1[i]);
            data[idx + 1] = ToByte(1.0f - worley2[i]);
            data[idx + 2] = ToByte(1.0f - worley3[i]);
            data[idx + 3] = 255;
        }

        return data;
    }

private:
    static uint8_t ToByte(float v) {
        return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
    }

    static float remap01(float v, float lo, float hi) {
        return std::clamp((v - lo) / (hi - lo), 0.0f, 1.0f);
    }

    // 3D Worley noise: returns values in [0,1] where 0 = at seed point, 1 = far from seeds
    static std::vector<float> Worley3D(int size, int cellCount, unsigned seed) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        // Generate seed points in each cell
        int totalCells = cellCount * cellCount * cellCount;
        std::vector<float> seedsX(totalCells), seedsY(totalCells), seedsZ(totalCells);
        for (int z = 0; z < cellCount; z++)
            for (int y = 0; y < cellCount; y++)
                for (int x = 0; x < cellCount; x++) {
                    int idx = z * cellCount * cellCount + y * cellCount + x;
                    seedsX[idx] = (x + dist(rng)) / float(cellCount);
                    seedsY[idx] = (y + dist(rng)) / float(cellCount);
                    seedsZ[idx] = (z + dist(rng)) / float(cellCount);
                }

        std::vector<float> result(size * size * size);
        float maxDist = 0.0f;

        for (int z = 0; z < size; z++)
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++) {
                    float px = float(x) / float(size);
                    float py = float(y) / float(size);
                    float pz = float(z) / float(size);

                    // Find which cell we're in
                    int cx = int(px * cellCount);
                    int cy = int(py * cellCount);
                    int cz = int(pz * cellCount);

                    float minDist = 1e10f;

                    // Check neighboring cells (3x3x3)
                    for (int dz = -1; dz <= 1; dz++)
                        for (int dy = -1; dy <= 1; dy++)
                            for (int dx = -1; dx <= 1; dx++) {
                                int nx = (cx + dx + cellCount) % cellCount;
                                int ny = (cy + dy + cellCount) % cellCount;
                                int nz = (cz + dz + cellCount) % cellCount;
                                int idx = nz * cellCount * cellCount + ny * cellCount + nx;

                                // Wrap distance for tileability
                                float sx = seedsX[idx] + float(dx < 0 && nx > cx ? -1 : (dx > 0 && nx < cx ? 1 : 0));
                                float sy = seedsY[idx] + float(dy < 0 && ny > cy ? -1 : (dy > 0 && ny < cy ? 1 : 0));
                                float sz = seedsZ[idx] + float(dz < 0 && nz > cz ? -1 : (dz > 0 && nz < cz ? 1 : 0));

                                float ddx = px - sx;
                                float ddy = py - sy;
                                float ddz = pz - sz;
                                float d = ddx * ddx + ddy * ddy + ddz * ddz;
                                minDist = std::min(minDist, d);
                            }

                    minDist = std::sqrt(minDist);
                    int idx = z * size * size + y * size + x;
                    result[idx] = minDist;
                    maxDist = std::max(maxDist, minDist);
                }

        // Normalize
        if (maxDist > 0.0f)
            for (auto& v : result) v /= maxDist;

        return result;
    }

    // 3D Perlin noise
    static std::vector<float> Perlin3D(int size, int freq, unsigned seed) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        // Generate gradient vectors at grid points
        int gridSize = freq + 1;
        int totalGridPts = gridSize * gridSize * gridSize;
        std::vector<float> gradX(totalGridPts), gradY(totalGridPts), gradZ(totalGridPts);
        for (int i = 0; i < totalGridPts; i++) {
            float gx = dist(rng), gy = dist(rng), gz = dist(rng);
            float len = std::sqrt(gx * gx + gy * gy + gz * gz);
            if (len > 0.001f) { gx /= len; gy /= len; gz /= len; }
            gradX[i] = gx; gradY[i] = gy; gradZ[i] = gz;
        }

        auto fade = [](float t) -> float {
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        };

        auto lerp = [](float a, float b, float t) -> float {
            return a + t * (b - a);
        };

        auto gradIdx = [&](int x, int y, int z) -> int {
            x = x % freq; y = y % freq; z = z % freq;
            return z * gridSize * gridSize + y * gridSize + x;
        };

        auto dotGrad = [&](int ix, int iy, int iz, float x, float y, float z) -> float {
            int idx = gradIdx(ix, iy, iz);
            float dx = x - float(ix), dy = y - float(iy), dz = z - float(iz);
            return gradX[idx] * dx + gradY[idx] * dy + gradZ[idx] * dz;
        };

        std::vector<float> result(size * size * size);

        for (int z = 0; z < size; z++)
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++) {
                    float px = float(x) / float(size) * float(freq);
                    float py = float(y) / float(size) * float(freq);
                    float pz = float(z) / float(size) * float(freq);

                    int x0 = int(std::floor(px)), y0 = int(std::floor(py)), z0 = int(std::floor(pz));
                    float fx = px - x0, fy = py - y0, fz = pz - z0;
                    float u = fade(fx), v = fade(fy), w = fade(fz);

                    float n000 = dotGrad(x0, y0, z0, px, py, pz);
                    float n100 = dotGrad(x0 + 1, y0, z0, px, py, pz);
                    float n010 = dotGrad(x0, y0 + 1, z0, px, py, pz);
                    float n110 = dotGrad(x0 + 1, y0 + 1, z0, px, py, pz);
                    float n001 = dotGrad(x0, y0, z0 + 1, px, py, pz);
                    float n101 = dotGrad(x0 + 1, y0, z0 + 1, px, py, pz);
                    float n011 = dotGrad(x0, y0 + 1, z0 + 1, px, py, pz);
                    float n111 = dotGrad(x0 + 1, y0 + 1, z0 + 1, px, py, pz);

                    float val = lerp(
                        lerp(lerp(n000, n100, u), lerp(n010, n110, u), v),
                        lerp(lerp(n001, n101, u), lerp(n011, n111, u), v),
                        w
                    );

                    int idx = z * size * size + y * size + x;
                    result[idx] = val * 0.5f + 0.5f; // map [-1,1] to [0,1]
                }

        return result;
    }
};

} // namespace Puluo
