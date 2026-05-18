#pragma once

#include "puluo/core/Math.h"
#include <memory>
#include <cstdint>

namespace Puluo {

// Forward declarations
class Shader;

// Temporal Anti-Aliasing (TAA) implementation.
// Applies sub-pixel jitter across frames and blends with history to achieve
// multi-sample anti-aliasing effect without the memory cost of MSAA.
class TAA {
public:
    TAA() = default;
    ~TAA() { Destroy(); }

    void Create(uint32_t width, uint32_t height);
    void Destroy();
    void Resize(uint32_t width, uint32_t height);

    // Resolve TAA: blend current frame with history using motion vectors.
    // Inputs:
    //   - currentColor: current frame color texture (with jitter applied)
    //   - depthTexture: current frame depth texture (for reprojection)
    //   - currentVP: current frame view-projection matrix (jittered)
    //   - prevVP: previous frame view-projection matrix (unjittered)
    //   - jitter/prevJitter: current and previous frame jitter offsets
    //   - emptyVAO: VAO for fullscreen triangle draw
    // Output: TAA-resolved color in m_OutputTexture
    void Resolve(uint32_t currentColor, uint32_t depthTexture,
                 const Mat4& currentVP, const Mat4& prevVP,
                 Vec2 jitter, Vec2 prevJitter,
                 uint32_t emptyVAO);

    uint32_t GetOutputTexture() const { return m_OutputTexture; }
    bool IsCreated() const { return m_FBO != 0; }

private:
    uint32_t m_FBO = 0;
    uint32_t m_OutputTexture = 0;   // Current frame TAA output
    uint32_t m_HistoryTexture = 0;  // Previous frame TAA result
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    std::shared_ptr<Shader> m_TAAShader;
};

} // namespace Puluo
