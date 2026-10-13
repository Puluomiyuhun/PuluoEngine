#pragma once
// Texture graph data model.
//
// Deliberately free of any engine / ImGui / OpenGL dependency: this layer knows
// nothing about how it is drawn or evaluated. That is what keeps the designer
// decoupled from PuluoEngine -- the dependency only ever runs one way, and only
// from the preview and UI layers.
//
// Architecture note (from how Substance Designer itself is built): Adobe's docs
// state that every node in SD's 200+ node Library is, at its lowest level, built
// out of a small set of "atomic" nodes. So the plan is NOT to hand-code 200
// kernels -- it is to implement the ~24 atomics in C++/GLSL and author the
// library as *data* (subgraph instances) on top. Subgraph instancing is
// therefore the next structural step, and the Graph/Node split below is shaped
// to accept it additively rather than by rewrite.

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace sd {

using NodeId = uint32_t;
inline constexpr NodeId kInvalidNode = 0xFFFFFFFFu;

// ---------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------
struct Rgba {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
};

using Value = std::variant<float, int, bool, Rgba>;

enum class ValueType { Float, Int, Bool, Color, Enum };

float asFloat(const Value& v, float fallback = 0.0f);
int   asInt(const Value& v, int fallback = 0);
bool  asBool(const Value& v, bool fallback = false);
Rgba  asRgba(const Value& v, Rgba fallback = {});

// A parameter declaration. `name` doubles as the GLSL uniform name so there is
// no implicit naming convention to get wrong between C++ and the shader.
struct ParamDecl {
    std::string              name;   // e.g. "uGamma" -- also the GLSL uniform
    std::string              label;  // e.g. "Gamma"  -- shown in the UI
    ValueType                type  = ValueType::Float;
    Value                    def   = 0.0f;
    float                    lo    = 0.0f;
    float                    hi    = 1.0f;
    std::vector<std::string> enumLabels;  // only for ValueType::Enum
};

// ---------------------------------------------------------------------------
// Atomic kernels
// ---------------------------------------------------------------------------
// Implemented so far. Roadmap, in rough order of usefulness:
//   Transformation2D, Blur, Normal, Curve, GradientMap, GrayscaleConversion,
//   HSL, ChannelShuffle, Warp, DirectionalWarp, DirectionalBlur, Sharpen,
//   Emboss, Distance, Bitmap, Svg, Text,
//   then the three programmable escape hatches:
//   PixelProcessor, ValueProcessor, FXMap.
// PixelProcessor should come early -- it unlocks most procedural generators
// without needing FXMap, which is quad-tree based and by far the hardest.
enum class Atomic : int {
    UniformColor = 0,
    Levels,
    Blend,
    Count
};

struct SlotDecl {
    std::string name;
    bool        optional = false;
};

struct AtomicDef {
    std::string            name;    // display name
    std::string            shader;  // file under assets/shaders/texgraph/
    std::vector<SlotDecl>  inputs;
    std::vector<ParamDecl> params;
};

const AtomicDef& atomicDef(Atomic a);
const char*      atomicName(Atomic a);

// ---------------------------------------------------------------------------
// PBR output
// ---------------------------------------------------------------------------
// Mirrors the channel set SD exposes through its output templates, trimmed to
// what the engine's pbr.frag actually consumes (slots 0..4).
enum class PbrChannel : int {
    BaseColor = 0,
    Normal,
    Roughness,
    Metallic,
    AO,
    Count
};
inline constexpr int kPbrChannelCount = static_cast<int>(PbrChannel::Count);

const char* pbrChannelName(PbrChannel c);

// ---------------------------------------------------------------------------
// Nodes
// ---------------------------------------------------------------------------
enum class NodeKind { Atomic, Output };

struct Link {
    NodeId src = kInvalidNode;
};

struct Node {
    NodeId      id     = kInvalidNode;
    NodeKind    kind   = NodeKind::Atomic;
    Atomic      atomic = Atomic::UniformColor;
    std::string label;

    std::vector<Link>            inputs;  // one entry per declared slot
    std::map<std::string, Value> params;  // overrides only; absent => default

    // Resolution is stored as log2 so a graph stays resolution independent --
    // the same graph renders at 256 or 4096 with no edits. SD does the same.
    int sizeLog2 = 9;  // 512

    float uiX = 0.0f, uiY = 0.0f;
};

class Graph {
public:
    Graph();

    NodeId addAtomic(Atomic a, float x, float y);
    void   remove(NodeId id);

    Node*       find(NodeId id);
    const Node* find(NodeId id) const;

    // Returns false when the slot index is out of range, or when the link would
    // introduce a cycle.
    bool connect(NodeId src, NodeId dst, int slot);
    void disconnect(NodeId dst, int slot);

    NodeId outputNode() const { return out_; }

    std::vector<Node>&       nodes()       { return nodes_; }
    const std::vector<Node>& nodes() const { return nodes_; }

    // Resolved parameter: the node's override if present, else the declared
    // default.
    Value param(const Node& n, const std::string& name) const;

    bool dirty = true;

private:
    std::vector<Node> nodes_;
    NodeId            next_ = 1;
    NodeId            out_  = kInvalidNode;

    bool reaches_(NodeId from, NodeId target) const;
};

// Slot introspection, uniform across atomic and output nodes.
int         slotCount(const Node& n);
std::string slotName(const Node& n, int slot);

}  // namespace sd
