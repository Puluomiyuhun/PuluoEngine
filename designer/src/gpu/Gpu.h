#pragma once
// Minimal GPU helpers for texture-graph evaluation.
//
// The engine's Shader class only builds vert/frag and vert/tesc/tese/frag
// pipelines, and its Texture2D is fixed to 8-bit RGBA loaded from disk or
// memory. Rather than widen either of those -- which would mean editing the
// engine and breaking the one-way dependency -- the designer carries its own
// 100-line compute/texture pair. glad is already linked, so this costs nothing.

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sd {

class ComputeShader {
public:
    ~ComputeShader();

    ComputeShader(const ComputeShader&)            = delete;
    ComputeShader& operator=(const ComputeShader&) = delete;

    // Returns nullptr and logs on compile/link failure.
    static std::shared_ptr<ComputeShader> fromFile(const std::string& path);

    void bind() const;

    void setInt(const char* name, int v) const;
    void setFloat(const char* name, float v) const;
    void setVec4(const char* name, float x, float y, float z, float w) const;
    void setIVec2(const char* name, int x, int y) const;

    // Dispatches ceil(size/8) groups on each axis; kernels declare
    // local_size_x = local_size_y = 8.
    void dispatch(int width, int height) const;

    uint32_t id() const { return prog_; }

private:
    ComputeShader() = default;

    uint32_t                          prog_ = 0;
    mutable std::map<std::string, int> locCache_;

    int loc(const char* name) const;
};

// Recycles square RGBA16F textures keyed by edge length. Graph evaluation
// allocates and frees a lot of identically sized intermediates, so reuse keeps
// us off the driver's allocator on every parameter tweak.
class TexturePool {
public:
    ~TexturePool();

    uint32_t acquire(int size);
    void     release(uint32_t tex);
    void     clear();

private:
    std::map<int, std::vector<uint32_t>> free_;
    std::map<uint32_t, int>              sizeOf_;
};

// 256-bin luminance histogram, used by the Levels histogram editor and its
// Auto Level action. Readback is synchronous -- it runs once per graph
// re-evaluation for a single selected node, not per frame.
class HistogramComputer {
public:
    static constexpr int kBins = 256;

    bool init(const std::string& shaderDir);
    void shutdown();

    // Fills bins[kBins]. Returns false if unavailable or tex is 0.
    bool compute(uint32_t tex, int size, uint32_t* bins);

private:
    std::shared_ptr<ComputeShader> shader_;
    uint32_t                       ssbo_ = 0;
};

}  // namespace sd
