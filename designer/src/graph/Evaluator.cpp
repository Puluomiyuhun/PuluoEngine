#include "Evaluator.h"

#include <glad/gl.h>

#include "puluo/core/Log.h"

namespace sd {

namespace {

constexpr int kMaxDepth = 64;  // guards against a cycle slipping past connect()

// Pack mode expected by output_pack.comp for each PBR channel.
int packModeFor(int channel) {
    switch (static_cast<PbrChannel>(channel)) {
        case PbrChannel::BaseColor: return 0;  // sRGB encode
        case PbrChannel::Normal:    return 2;  // passthrough
        default:                    return 1;  // grayscale broadcast
    }
}

uint32_t createTexture(int size) {
    GLuint tex = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_RGBA16F, size, size);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

}  // namespace

bool Evaluator::init(const std::string& shaderDir) {
    shaders_.assign(static_cast<size_t>(Atomic::Count), nullptr);

    bool allOk = true;
    for (int i = 0; i < static_cast<int>(Atomic::Count); ++i) {
        const auto& def = atomicDef(static_cast<Atomic>(i));
        auto cs = ComputeShader::fromFile(shaderDir + "/" + def.shader);
        if (!cs) {
            PULUO_CORE_ERROR("[texgraph] atomic '{}' unavailable", def.name);
            allOk = false;
            continue;
        }
        shaders_[static_cast<size_t>(i)] = std::move(cs);
    }

    pack_ = ComputeShader::fromFile(shaderDir + "/output_pack.comp");
    if (!pack_) {
        PULUO_CORE_ERROR("[texgraph] output packing pass unavailable");
        allOk = false;
    }

    return allOk;
}

void Evaluator::shutdown() {
    for (int c = 0; c < kPbrChannelCount; ++c) {
        if (outTex_[c]) {
            GLuint t = outTex_[c];
            glDeleteTextures(1, &t);
            outTex_[c]  = 0;
            outSize_[c] = 0;
        }
    }
    shaders_.clear();
    pack_.reset();
    live_.clear();
    pool_.clear();
}

uint32_t Evaluator::evalNode_(const Graph& g, NodeId id,
                              std::map<NodeId, uint32_t>& memo, int depth) {
    if (id == kInvalidNode || depth > kMaxDepth) return 0;

    if (auto it = memo.find(id); it != memo.end()) return it->second;

    const Node* n = g.find(id);
    if (!n || n->kind != NodeKind::Atomic) return 0;

    auto& shader = shaders_[static_cast<size_t>(n->atomic)];
    if (!shader) return 0;

    const AtomicDef& def   = atomicDef(n->atomic);
    const int        slots = static_cast<int>(def.inputs.size());

    // Resolve inputs depth-first so they are ready to sample.
    uint32_t inTex[8] = {};
    for (int s = 0; s < slots && s < 8; ++s)
        inTex[s] = evalNode_(g, n->inputs[static_cast<size_t>(s)].src, memo, depth + 1);

    // A required slot with nothing upstream leaves the node undefined.
    for (int s = 0; s < slots; ++s) {
        if (!def.inputs[static_cast<size_t>(s)].optional && inTex[s] == 0) {
            memo[id] = 0;
            return 0;
        }
    }

    const int size = 1 << n->sizeLog2;
    uint32_t  out  = pool_.acquire(size);
    live_.push_back(out);

    shader->bind();
    shader->setIVec2("uSize", size, size);
    glBindImageTexture(0, out, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    char name[16];
    for (int s = 0; s < slots && s < 8; ++s) {
        glBindTextureUnit(static_cast<GLuint>(s), inTex[s]);
        snprintf(name, sizeof(name), "uIn%d", s);
        shader->setInt(name, s);
        snprintf(name, sizeof(name), "uHas%d", s);
        shader->setInt(name, inTex[s] ? 1 : 0);
    }

    for (const auto& p : def.params) {
        const Value v = g.param(*n, p.name);
        switch (p.type) {
            case ValueType::Float:
                shader->setFloat(p.name.c_str(), asFloat(v));
                break;
            case ValueType::Int:
            case ValueType::Enum:
                shader->setInt(p.name.c_str(), asInt(v));
                break;
            case ValueType::Bool:
                shader->setInt(p.name.c_str(), asBool(v) ? 1 : 0);
                break;
            case ValueType::Color: {
                const Rgba c = asRgba(v);
                shader->setVec4(p.name.c_str(), c.r, c.g, c.b, c.a);
                break;
            }
        }
    }

    shader->dispatch(size, size);

    memo[id] = out;
    return out;
}

uint32_t Evaluator::packInto_(int channel, uint32_t src, int size) {
    if (!pack_ || !src) return 0;

    if (outTex_[channel] && outSize_[channel] != size) {
        GLuint t = outTex_[channel];
        glDeleteTextures(1, &t);
        outTex_[channel] = 0;
    }
    if (!outTex_[channel]) {
        outTex_[channel]  = createTexture(size);
        outSize_[channel] = size;
    }

    pack_->bind();
    pack_->setIVec2("uSize", size, size);
    pack_->setInt("uPackMode", packModeFor(channel));
    glBindImageTexture(0, outTex_[channel], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindTextureUnit(0, src);
    pack_->setInt("uIn0", 0);
    pack_->dispatch(size, size);

    return outTex_[channel];
}

EvalResult Evaluator::evaluate(const Graph& g) {
    // Recycle the previous frame's intermediates. The final channel textures
    // are owned separately, so they stay valid across this boundary.
    for (uint32_t t : live_) pool_.release(t);
    live_.clear();
    nodeTex_.clear();

    EvalResult result;

    // Evaluate every atomic, not just what the output consumes: SD shows a
    // thumbnail on every node regardless of whether it is wired to an output,
    // and the 2D viewer needs to inspect dead-end branches too. nodeTex_
    // doubles as the memoisation table, so shared subtrees still run once.
    for (const auto& n : g.nodes())
        if (n.kind == NodeKind::Atomic) evalNode_(g, n.id, nodeTex_, 0);

    const Node* out = g.find(g.outputNode());
    if (!out) {
        glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glUseProgram(0);
        return result;
    }

    const int size = 1 << out->sizeLog2;
    for (int c = 0; c < kPbrChannelCount; ++c) {
        if (c >= static_cast<int>(out->inputs.size())) break;
        const NodeId src = out->inputs[static_cast<size_t>(c)].src;
        result.channel[c] = packInto_(c, nodeTexture(src), size);
    }

    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glUseProgram(0);
    return result;
}

}  // namespace sd
