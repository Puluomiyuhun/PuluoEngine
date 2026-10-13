#pragma once
// Graph evaluation on the GPU.
//
// Walks back from the output node, evaluating each reachable atomic into a
// pooled RGBA16F texture. Shared results are memoized, so a node feeding three
// consumers is dispatched once.
//
// Intermediates live in a pool and are recycled on the next evaluate(). The
// five *final* channel textures are owned persistently, because they are handed
// to the renderer and sampled for the whole frame.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../gpu/Gpu.h"
#include "Graph.h"

namespace sd {

struct EvalResult {
    // GL texture names per PBR channel; 0 means "nothing connected".
    uint32_t channel[kPbrChannelCount] = {};
};

class Evaluator {
public:
    // shaderDir is e.g. "assets/shaders/texgraph". Returns false if any kernel
    // failed to build, after logging which.
    bool init(const std::string& shaderDir);
    void shutdown();

    EvalResult evaluate(const Graph& g);

    // Per-node results from the last evaluate(), for node thumbnails and the
    // 2D viewer. Valid until the next evaluate() call.
    const std::map<NodeId, uint32_t>& nodeTextures() const { return nodeTex_; }

    uint32_t nodeTexture(NodeId id) const {
        auto it = nodeTex_.find(id);
        return it == nodeTex_.end() ? 0u : it->second;
    }

private:
    TexturePool                                 pool_;
    std::vector<std::shared_ptr<ComputeShader>> shaders_;  // indexed by Atomic
    std::shared_ptr<ComputeShader>              pack_;
    std::vector<uint32_t>                       live_;     // recycled next eval
    std::map<NodeId, uint32_t>                  nodeTex_;

    // Persistent, renderer-facing outputs.
    uint32_t outTex_[kPbrChannelCount]  = {};
    int      outSize_[kPbrChannelCount] = {};

    uint32_t evalNode_(const Graph& g, NodeId id,
                       std::map<NodeId, uint32_t>& memo, int depth);
    uint32_t packInto_(int channel, uint32_t src, int size);
};

}  // namespace sd
