#include "Graph.h"

#include <array>

namespace sd {

// ---------------------------------------------------------------------------
// Value accessors
// ---------------------------------------------------------------------------
float asFloat(const Value& v, float fallback) {
    if (auto* f = std::get_if<float>(&v)) return *f;
    if (auto* i = std::get_if<int>(&v)) return static_cast<float>(*i);
    if (auto* b = std::get_if<bool>(&v)) return *b ? 1.0f : 0.0f;
    return fallback;
}

int asInt(const Value& v, int fallback) {
    if (auto* i = std::get_if<int>(&v)) return *i;
    if (auto* f = std::get_if<float>(&v)) return static_cast<int>(*f);
    if (auto* b = std::get_if<bool>(&v)) return *b ? 1 : 0;
    return fallback;
}

bool asBool(const Value& v, bool fallback) {
    if (auto* b = std::get_if<bool>(&v)) return *b;
    if (auto* i = std::get_if<int>(&v)) return *i != 0;
    if (auto* f = std::get_if<float>(&v)) return *f != 0.0f;
    return fallback;
}

Rgba asRgba(const Value& v, Rgba fallback) {
    if (auto* c = std::get_if<Rgba>(&v)) return *c;
    if (auto* f = std::get_if<float>(&v)) return Rgba{*f, *f, *f, 1.0f};
    return fallback;
}

// ---------------------------------------------------------------------------
// Atomic definitions
// ---------------------------------------------------------------------------
static std::array<AtomicDef, static_cast<size_t>(Atomic::Count)> buildAtomicTable() {
    std::array<AtomicDef, static_cast<size_t>(Atomic::Count)> t;

    // --- Uniform Color -----------------------------------------------------
    {
        AtomicDef d;
        d.name   = "Uniform Color";
        d.shader = "uniform_color.comp";
        d.params = {
            {"uColor", "Color", ValueType::Color, Rgba{0.5f, 0.5f, 0.5f, 1.0f}},
        };
        t[static_cast<size_t>(Atomic::UniformColor)] = std::move(d);
    }

    // --- Levels ------------------------------------------------------------
    {
        AtomicDef d;
        d.name   = "Levels";
        d.shader = "levels.comp";
        d.inputs = {{"Input", false}};
        // Parameter set and naming follow SD's Levels node. "in mid" is the
        // input level that maps to mid grey, not a gamma exponent.
        d.params = {
            {"uInLow",   "Level in low",   ValueType::Float, 0.0f, 0.0f, 1.0f},
            {"uInMid",   "Level in mid",   ValueType::Float, 0.5f, 0.0f, 1.0f},
            {"uInHigh",  "Level in high",  ValueType::Float, 1.0f, 0.0f, 1.0f},
            {"uOutLow",  "Level out low",  ValueType::Float, 0.0f, 0.0f, 1.0f},
            {"uOutHigh", "Level out high", ValueType::Float, 1.0f, 0.0f, 1.0f},
            {"uClamp",   "Intermediary clamp", ValueType::Bool, true},
        };
        t[static_cast<size_t>(Atomic::Levels)] = std::move(d);
    }

    // --- Blend -------------------------------------------------------------
    {
        AtomicDef d;
        d.name   = "Blend";
        d.shader = "blend.comp";
        // Slot order matches SD: foreground over background, optional mask.
        d.inputs = {{"Foreground", true}, {"Background", true}, {"Mask", true}};
        ParamDecl mode;
        mode.name       = "uMode";
        mode.label      = "Blending Mode";
        mode.type       = ValueType::Enum;
        mode.def        = 0;
        mode.enumLabels = {"Copy",     "Add",      "Subtract", "Multiply",
                           "Max",      "Min",      "Screen",   "Overlay"};
        d.params = {
            std::move(mode),
            {"uOpacity", "Opacity", ValueType::Float, 1.0f, 0.0f, 1.0f},
        };
        t[static_cast<size_t>(Atomic::Blend)] = std::move(d);
    }

    return t;
}

const AtomicDef& atomicDef(Atomic a) {
    static const auto table = buildAtomicTable();
    size_t i = static_cast<size_t>(a);
    if (i >= table.size()) i = 0;
    return table[i];
}

const char* atomicName(Atomic a) { return atomicDef(a).name.c_str(); }

const char* pbrChannelName(PbrChannel c) {
    switch (c) {
        case PbrChannel::BaseColor: return "Base Color";
        case PbrChannel::Normal:    return "Normal";
        case PbrChannel::Roughness: return "Roughness";
        case PbrChannel::Metallic:  return "Metallic";
        case PbrChannel::AO:        return "Ambient Occlusion";
        default:                    return "?";
    }
}

// ---------------------------------------------------------------------------
// Slot introspection
// ---------------------------------------------------------------------------
int slotCount(const Node& n) {
    if (n.kind == NodeKind::Output) return kPbrChannelCount;
    return static_cast<int>(atomicDef(n.atomic).inputs.size());
}

std::string slotName(const Node& n, int slot) {
    if (n.kind == NodeKind::Output) {
        if (slot < 0 || slot >= kPbrChannelCount) return "?";
        return pbrChannelName(static_cast<PbrChannel>(slot));
    }
    const auto& in = atomicDef(n.atomic).inputs;
    if (slot < 0 || slot >= static_cast<int>(in.size())) return "?";
    return in[slot].name;
}

// ---------------------------------------------------------------------------
// Graph
// ---------------------------------------------------------------------------
Graph::Graph() {
    // Every graph has exactly one output node, created up front and not
    // removable -- it is the root the evaluator walks back from.
    Node out;
    out.id    = next_++;
    out.kind  = NodeKind::Output;
    out.label = "Output";
    out.inputs.resize(kPbrChannelCount);
    out.uiX = 620.0f;
    out.uiY = 180.0f;
    out_    = out.id;
    nodes_.push_back(std::move(out));
}

NodeId Graph::addAtomic(Atomic a, float x, float y) {
    Node n;
    n.id     = next_++;
    n.kind   = NodeKind::Atomic;
    n.atomic = a;
    n.label  = atomicDef(a).name;
    n.inputs.resize(atomicDef(a).inputs.size());
    n.uiX = x;
    n.uiY = y;
    nodes_.push_back(std::move(n));
    dirty = true;
    return nodes_.back().id;
}

void Graph::remove(NodeId id) {
    if (id == out_) return;  // the output node is permanent
    for (auto& n : nodes_)
        for (auto& l : n.inputs)
            if (l.src == id) l.src = kInvalidNode;

    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].id == id) {
            nodes_.erase(nodes_.begin() + static_cast<ptrdiff_t>(i));
            break;
        }
    }
    dirty = true;
}

Node* Graph::find(NodeId id) {
    for (auto& n : nodes_)
        if (n.id == id) return &n;
    return nullptr;
}

const Node* Graph::find(NodeId id) const {
    for (auto& n : nodes_)
        if (n.id == id) return &n;
    return nullptr;
}

// Depth-first reachability, used for cycle rejection.
bool Graph::reaches_(NodeId from, NodeId target) const {
    if (from == kInvalidNode) return false;
    if (from == target) return true;
    const Node* n = find(from);
    if (!n) return false;
    for (const auto& l : n->inputs)
        if (reaches_(l.src, target)) return true;
    return false;
}

bool Graph::connect(NodeId src, NodeId dst, int slot) {
    Node* d = find(dst);
    if (!d || !find(src)) return false;
    if (slot < 0 || slot >= static_cast<int>(d->inputs.size())) return false;
    if (src == dst) return false;
    // Reject if dst already feeds src -- that would close a loop.
    if (reaches_(src, dst)) return false;

    d->inputs[static_cast<size_t>(slot)].src = src;
    dirty = true;
    return true;
}

void Graph::disconnect(NodeId dst, int slot) {
    Node* d = find(dst);
    if (!d) return;
    if (slot < 0 || slot >= static_cast<int>(d->inputs.size())) return;
    d->inputs[static_cast<size_t>(slot)].src = kInvalidNode;
    dirty = true;
}

Value Graph::param(const Node& n, const std::string& name) const {
    auto it = n.params.find(name);
    if (it != n.params.end()) return it->second;
    if (n.kind == NodeKind::Atomic) {
        for (const auto& p : atomicDef(n.atomic).params)
            if (p.name == name) return p.def;
    }
    return 0.0f;
}

}  // namespace sd
