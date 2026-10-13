#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

#include "Ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace sd {
namespace {

// Base (unzoomed) geometry. SD draws a node as a square thumbnail with its
// name underneath and pins on the left/right edges, which is what this mirrors.
constexpr float kNodeSide = 80.0f;
constexpr float kLabelH   = 17.0f;
constexpr float kPinR     = 4.5f;
constexpr float kGrabPad  = 4.0f;

constexpr float kZoomMin = 0.3f;
constexpr float kZoomMax = 3.0f;

ImU32 col(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }

// Nodes are tinted by role, the way SD separates generators from filters from
// compositing -- it keeps a dense graph readable at a glance.
ImU32 accentColor(const Node& n) {
    if (n.kind == NodeKind::Output) return col(176, 118, 74);
    switch (n.atomic) {
        case Atomic::UniformColor: return col(84, 142, 126);
        case Atomic::Levels:       return col(104, 128, 168);
        case Atomic::Blend:        return col(168, 140, 92);
        default:                   return col(110, 114, 122);
    }
}

float nodeSide(const CanvasState& st) { return kNodeSide * st.zoom; }
float pinRadius(const CanvasState& st) { return std::max(3.0f, kPinR * st.zoom); }

ImVec2 nodeTopLeft(const Node& n, ImVec2 origin, const CanvasState& st) {
    return ImVec2(origin.x + n.uiX * st.zoom + st.panX,
                  origin.y + n.uiY * st.zoom + st.panY);
}

ImVec2 inPinPos(const Node& n, ImVec2 origin, const CanvasState& st, int slot) {
    const ImVec2 tl = nodeTopLeft(n, origin, st);
    const float  s  = nodeSide(st);
    const int    cnt = std::max(slotCount(n), 1);
    const float  t   = static_cast<float>(slot + 1) / static_cast<float>(cnt + 1);
    return ImVec2(tl.x, tl.y + s * t);
}

ImVec2 outPinPos(const Node& n, ImVec2 origin, const CanvasState& st) {
    const ImVec2 tl = nodeTopLeft(n, origin, st);
    const float  s  = nodeSide(st);
    return ImVec2(tl.x + s, tl.y + s * 0.5f);
}

bool hitNode(const Node& n, ImVec2 origin, const CanvasState& st, ImVec2 p) {
    const ImVec2 tl = nodeTopLeft(n, origin, st);
    const float  s  = nodeSide(st);
    return p.x >= tl.x && p.x <= tl.x + s && p.y >= tl.y &&
           p.y <= tl.y + s + kLabelH * st.zoom;
}

bool nearPin(ImVec2 a, ImVec2 b, const CanvasState& st) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    const float r  = pinRadius(st) + kGrabPad;
    return dx * dx + dy * dy <= r * r;
}

void drawLink(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 c, float z) {
    const float dx = std::max(32.0f * z, std::abs(b.x - a.x) * 0.5f);
    dl->AddBezierCubic(a, ImVec2(a.x + dx, a.y), ImVec2(b.x - dx, b.y), b, c,
                       std::max(1.4f, 2.0f * z));
}

void drawZoomedText(ImDrawList* dl, ImVec2 pos, ImU32 c, const char* txt, float z) {
    const float fs = std::clamp(ImGui::GetFontSize() * z, 7.0f, 40.0f);
    dl->AddText(ImGui::GetFont(), fs, pos, c, txt);
}

ImVec2 zoomedTextSize(const char* txt, float z) {
    const float fs = std::clamp(ImGui::GetFontSize() * z, 7.0f, 40.0f);
    return ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0.0f, txt);
}

void drawGrid(ImDrawList* dl, ImVec2 origin, ImVec2 size, const CanvasState& st) {
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                      col(26, 27, 29));

    const float minor = 24.0f * st.zoom;
    if (minor < 6.0f) return;  // too dense to be useful

    const float major = minor * 5.0f;
    for (float x = std::fmod(st.panX, major); x < size.x; x += major)
        dl->AddLine(ImVec2(origin.x + x, origin.y),
                    ImVec2(origin.x + x, origin.y + size.y), col(43, 44, 48));
    for (float y = std::fmod(st.panY, major); y < size.y; y += major)
        dl->AddLine(ImVec2(origin.x, origin.y + y),
                    ImVec2(origin.x + size.x, origin.y + y), col(43, 44, 48));
}

// Checkerboard, so texture alpha and pure black reads are distinguishable.
void drawCheckerboard(ImDrawList* dl, ImVec2 a, ImVec2 b, float cell) {
    dl->AddRectFilled(a, b, col(58, 58, 62));
    int row = 0;
    for (float y = a.y; y < b.y; y += cell, ++row) {
        int cI = row;
        for (float x = a.x; x < b.x; x += cell, ++cI) {
            if (cI % 2) continue;
            dl->AddRectFilled(ImVec2(x, y),
                              ImVec2(std::min(x + cell, b.x), std::min(y + cell, b.y)),
                              col(72, 72, 77));
        }
    }
}

ImTextureID texId(uint32_t tex) {
    return static_cast<ImTextureID>(static_cast<uintptr_t>(tex));
}

// ---------------------------------------------------------------------------
// Levels histogram editor
// ---------------------------------------------------------------------------
// Models SD's histogram interface: the input range is set by three handles
// (low / mid / high) under a histogram, and the output range by two handles
// under a gradient bar. SD also offers a pure-slider interface for exact values
// and parameter exposure, which the toggle below mirrors.
bool drawLevelsEditor(Graph& g, Node& n, CanvasState& st, const PanelContext& ctx) {
    bool changed = false;

    float inLow   = asFloat(g.param(n, "uInLow"), 0.0f);
    float inMid   = asFloat(g.param(n, "uInMid"), 0.5f);
    float inHigh  = asFloat(g.param(n, "uInHigh"), 1.0f);
    float outLow  = asFloat(g.param(n, "uOutLow"), 0.0f);
    float outHigh = asFloat(g.param(n, "uOutHigh"), 1.0f);

    auto store = [&](const char* k, float v) {
        n.params[k] = v;
        changed     = true;
    };

    // --- header actions ----------------------------------------------------
    if (ImGui::SmallButton("Invert")) {
        store("uOutLow", outHigh);
        store("uOutHigh", outLow);
        std::swap(outLow, outHigh);
    }
    ImGui::SameLine();
    const bool canAuto = ctx.histBins != nullptr;
    if (!canAuto) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Auto Level")) {
        int lo = -1, hi = -1;
        for (int i = 0; i < 256; ++i)
            if (ctx.histBins[i] > 0) { if (lo < 0) lo = i; hi = i; }
        if (lo >= 0 && hi > lo) {
            inLow  = static_cast<float>(lo) / 255.0f;
            inHigh = static_cast<float>(hi) / 255.0f;
            store("uInLow", inLow);
            store("uInHigh", inHigh);
        }
    }
    if (!canAuto) ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::SmallButton(st.levelsSliders ? "Histogram" : "Sliders"))
        st.levelsSliders = !st.levelsSliders;

    ImGui::Spacing();

    if (st.levelsSliders) {
        if (ImGui::SliderFloat("Level in low", &inLow, 0.0f, 1.0f))   store("uInLow", inLow);
        if (ImGui::SliderFloat("Level in mid", &inMid, 0.0f, 1.0f))   store("uInMid", inMid);
        if (ImGui::SliderFloat("Level in high", &inHigh, 0.0f, 1.0f)) store("uInHigh", inHigh);
        ImGui::Spacing();
        if (ImGui::SliderFloat("Level out low", &outLow, 0.0f, 1.0f))   store("uOutLow", outLow);
        if (ImGui::SliderFloat("Level out high", &outHigh, 0.0f, 1.0f)) store("uOutHigh", outHigh);
    } else {
        const float w     = std::max(120.0f, ImGui::GetContentRegionAvail().x);
        const float histH = 86.0f;
        const float stripH = 13.0f;
        const float barH   = 13.0f;
        const float totalH = histH + stripH + 6.0f + barH + stripH;

        const ImVec2 tl = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##levels", ImVec2(w, totalH));
        const bool hovered = ImGui::IsItemHovered();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImGuiIO&    io = ImGui::GetIO();

        const float histTop   = tl.y;
        const float histBot   = histTop + histH;
        const float inStripY  = histBot;
        const float barTop    = inStripY + stripH + 6.0f;
        const float barBot    = barTop + barH;
        const float outStripY = barBot;

        auto xOf = [&](float v) { return tl.x + std::clamp(v, 0.0f, 1.0f) * w; };
        auto vOf = [&](float x) { return std::clamp((x - tl.x) / w, 0.0f, 1.0f); };

        // histogram background + bars
        dl->AddRectFilled(ImVec2(tl.x, histTop), ImVec2(tl.x + w, histBot),
                          col(24, 25, 28), 2.0f);
        if (ctx.histBins && ctx.histMax > 0) {
            // Log scale: a procedural texture is often dominated by one value,
            // which on a linear scale flattens everything else to nothing.
            const float denom = std::log(1.0f + static_cast<float>(ctx.histMax));
            for (int i = 0; i < 256; ++i) {
                const float frac =
                    std::log(1.0f + static_cast<float>(ctx.histBins[i])) / denom;
                if (frac <= 0.0f) continue;
                const float x0 = tl.x + (static_cast<float>(i) / 256.0f) * w;
                const float x1 = tl.x + (static_cast<float>(i + 1) / 256.0f) * w;
                dl->AddRectFilled(ImVec2(x0, histBot - frac * (histH - 2.0f)),
                                  ImVec2(std::max(x1, x0 + 1.0f), histBot),
                                  col(120, 128, 140));
            }
        } else {
            const char* msg = "no input";
            const ImVec2 ts = ImGui::CalcTextSize(msg);
            dl->AddText(ImVec2(tl.x + (w - ts.x) * 0.5f, histTop + histH * 0.5f - 7.0f),
                        col(96, 100, 108), msg);
        }

        // transfer curve overlay -- shows what the five handles actually do
        const float gamma = std::log(0.5f) / std::log(std::clamp(inMid, 0.001f, 0.999f));
        const float span  = std::max(inHigh - inLow, 1e-5f);
        ImVec2 prev;
        for (int i = 0; i <= 64; ++i) {
            const float x = static_cast<float>(i) / 64.0f;
            float v = std::clamp((x - inLow) / span, 0.0f, 1.0f);
            v = std::pow(v, gamma);
            v = outLow + v * (outHigh - outLow);
            const ImVec2 p(tl.x + x * w,
                           histBot - std::clamp(v, 0.0f, 1.0f) * (histH - 2.0f));
            if (i > 0) dl->AddLine(prev, p, col(226, 196, 120), 1.6f);
            prev = p;
        }
        dl->AddRect(ImVec2(tl.x, histTop), ImVec2(tl.x + w, histBot),
                    col(16, 17, 19), 2.0f);

        // output gradient bar, drawn through the current mapping
        for (int i = 0; i < 64; ++i) {
            const float t0 = static_cast<float>(i) / 64.0f;
            float v = std::clamp((t0 - inLow) / span, 0.0f, 1.0f);
            v = std::pow(v, gamma);
            v = std::clamp(outLow + v * (outHigh - outLow), 0.0f, 1.0f);
            const int g8 = static_cast<int>(v * 255.0f);
            dl->AddRectFilled(ImVec2(tl.x + t0 * w, barTop),
                              ImVec2(tl.x + (t0 + 1.0f / 64.0f) * w + 1.0f, barBot),
                              col(g8, g8, g8));
        }
        dl->AddRect(ImVec2(tl.x, barTop), ImVec2(tl.x + w, barBot), col(16, 17, 19));

        // handles
        struct Handle { float v; float y; ImU32 c; };
        const Handle handles[5] = {
            {inLow,   inStripY,  col(28, 28, 30)},
            {inMid,   inStripY,  col(140, 140, 146)},
            {inHigh,  inStripY,  col(238, 238, 242)},
            {outLow,  outStripY, col(28, 28, 30)},
            {outHigh, outStripY, col(238, 238, 242)},
        };

        for (int i = 0; i < 5; ++i) {
            const float x = xOf(handles[i].v);
            const float y = handles[i].y;
            const ImVec2 a(x, y + 1.0f);
            const ImVec2 b(x - 5.0f, y + stripH - 1.0f);
            const ImVec2 c(x + 5.0f, y + stripH - 1.0f);
            dl->AddTriangleFilled(a, b, c, handles[i].c);
            dl->AddTriangle(a, b, c,
                            st.levelsDrag == i ? col(226, 196, 120) : col(16, 17, 19),
                            1.4f);
        }

        // drag handling
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const bool inInput  = io.MousePos.y >= inStripY - 2.0f &&
                                 io.MousePos.y <= inStripY + stripH + 2.0f;
            const bool inOutput = io.MousePos.y >= outStripY - 2.0f &&
                                 io.MousePos.y <= outStripY + stripH + 2.0f;
            int   best = -1;
            float bestD = 1e9f;
            for (int i = 0; i < 5; ++i) {
                const bool isInput = i < 3;
                if (isInput ? !inInput : !inOutput) continue;
                const float d = std::abs(xOf(handles[i].v) - io.MousePos.x);
                if (d < bestD) { bestD = d; best = i; }
            }
            if (best >= 0 && bestD < 14.0f) st.levelsDrag = best;
        }

        if (st.levelsDrag >= 0) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                const float v = vOf(io.MousePos.x);
                switch (st.levelsDrag) {
                    // Keep low strictly below high; mid is independent in SD.
                    case 0: store("uInLow", std::min(v, inHigh - 0.001f)); break;
                    case 1: store("uInMid", std::clamp(v, 0.001f, 0.999f)); break;
                    case 2: store("uInHigh", std::max(v, inLow + 0.001f)); break;
                    case 3: store("uOutLow", v); break;
                    case 4: store("uOutHigh", v); break;
                    default: break;
                }
            } else {
                st.levelsDrag = -1;
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("in %.3f / %.3f / %.3f    out %.3f / %.3f", inLow, inMid,
                            inHigh, outLow, outHigh);
    }

    bool clampOn = asBool(g.param(n, "uClamp"), true);
    if (ImGui::Checkbox("Intermediary clamp", &clampOn)) {
        n.params["uClamp"] = clampOn;
        changed            = true;
    }

    return changed;
}

}  // namespace

// ---------------------------------------------------------------------------
// Canvas
// ---------------------------------------------------------------------------
void drawNodeCanvas(const char* strId, Graph& g, CanvasState& st,
                    const std::map<NodeId, uint32_t>& thumbs) {
    ImGui::BeginChild(strId, ImVec2(0, 0), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2       size   = ImGui::GetContentRegionAvail();
    size.x = std::max(size.x, 50.0f);
    size.y = std::max(size.y, 50.0f);

    ImGui::InvisibleButton("##canvas", size,
                           ImGuiButtonFlags_MouseButtonLeft |
                           ImGuiButtonFlags_MouseButtonRight |
                           ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();

    ImGuiIO&     io = ImGui::GetIO();
    const ImVec2 mp = io.MousePos;

    // ---- wheel zoom, anchored on the cursor --------------------------------
    if (hovered && io.MouseWheel != 0.0f && st.mode == DragMode::None) {
        const float before = st.zoom;
        st.zoom = std::clamp(st.zoom * std::pow(1.12f, io.MouseWheel), kZoomMin, kZoomMax);
        if (st.zoom != before) {
            // Keep the graph point under the cursor fixed.
            const float gx = (mp.x - origin.x - st.panX) / before;
            const float gy = (mp.y - origin.y - st.panY) / before;
            st.panX = mp.x - origin.x - gx * st.zoom;
            st.panY = mp.y - origin.y - gy * st.zoom;
        }
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);

    drawGrid(dl, origin, size, st);

    // ---- press -------------------------------------------------------------
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        bool consumed = false;

        for (auto it = g.nodes().rbegin(); it != g.nodes().rend() && !consumed; ++it) {
            if (it->kind == NodeKind::Output) continue;  // no output pin
            if (nearPin(mp, outPinPos(*it, origin, st), st)) {
                st.mode    = DragMode::NewLink;
                st.linkSrc = it->id;
                consumed   = true;
            }
        }

        for (auto it = g.nodes().rbegin(); it != g.nodes().rend() && !consumed; ++it) {
            const int slots = slotCount(*it);
            for (int s = 0; s < slots; ++s) {
                if (nearPin(mp, inPinPos(*it, origin, st, s), st)) {
                    g.disconnect(it->id, s);
                    consumed = true;
                    break;
                }
            }
        }

        for (auto it = g.nodes().rbegin(); it != g.nodes().rend() && !consumed; ++it) {
            if (!hitNode(*it, origin, st, mp)) continue;
            const ImVec2 tl = nodeTopLeft(*it, origin, st);
            st.selected = it->id;
            st.mode     = DragMode::MoveNode;
            st.dragNode = it->id;
            st.grabDX   = mp.x - tl.x;
            st.grabDY   = mp.y - tl.y;
            consumed    = true;
        }

        if (!consumed) st.selected = kInvalidNode;
    }

    // Double-click opens the node in the 2D viewer, as in SD.
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        for (auto it = g.nodes().rbegin(); it != g.nodes().rend(); ++it) {
            if (hitNode(*it, origin, st, mp)) {
                st.previewNode = it->id;
                break;
            }
        }
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
        st.mode = DragMode::Pan;

    // ---- drag --------------------------------------------------------------
    if (st.mode == DragMode::Pan) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            st.panX += io.MouseDelta.x;
            st.panY += io.MouseDelta.y;
        } else {
            st.mode = DragMode::None;
        }
    } else if (st.mode == DragMode::MoveNode) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (Node* n = g.find(st.dragNode)) {
                n->uiX = (mp.x - origin.x - st.panX - st.grabDX) / st.zoom;
                n->uiY = (mp.y - origin.y - st.panY - st.grabDY) / st.zoom;
            }
        } else {
            st.mode     = DragMode::None;
            st.dragNode = kInvalidNode;
        }
    } else if (st.mode == DragMode::NewLink) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            bool done = false;
            for (auto it = g.nodes().rbegin(); it != g.nodes().rend() && !done; ++it) {
                const int slots = slotCount(*it);
                for (int s = 0; s < slots; ++s) {
                    if (nearPin(mp, inPinPos(*it, origin, st, s), st)) {
                        g.connect(st.linkSrc, it->id, s);
                        done = true;
                        break;
                    }
                }
            }
            st.mode    = DragMode::None;
            st.linkSrc = kInvalidNode;
        }
    }

    // ---- links -------------------------------------------------------------
    for (const auto& n : g.nodes()) {
        const int slots = slotCount(n);
        for (int s = 0; s < slots; ++s) {
            const NodeId src = n.inputs[static_cast<size_t>(s)].src;
            if (src == kInvalidNode) continue;
            if (const Node* sn = g.find(src))
                drawLink(dl, outPinPos(*sn, origin, st), inPinPos(n, origin, st, s),
                         col(168, 172, 180), st.zoom);
        }
    }

    if (st.mode == DragMode::NewLink) {
        if (const Node* sn = g.find(st.linkSrc))
            drawLink(dl, outPinPos(*sn, origin, st), mp, col(226, 196, 120), st.zoom);
    }

    // ---- nodes -------------------------------------------------------------
    const float s   = nodeSide(st);
    const float pr  = pinRadius(st);
    const float lblH = kLabelH * st.zoom;

    for (const auto& n : g.nodes()) {
        const ImVec2 tl = nodeTopLeft(n, origin, st);
        const ImVec2 br(tl.x + s, tl.y + s);

        // Cull offscreen nodes -- cheap, and the graph can get large.
        if (br.x < origin.x || br.y < origin.y || tl.x > origin.x + size.x ||
            tl.y > origin.y + size.y)
            continue;

        const bool   sel    = (n.id == st.selected);
        const bool   inView = (n.id == st.previewNode);
        const ImU32  accent = accentColor(n);

        drawCheckerboard(dl, tl, br, std::max(5.0f, 8.0f * st.zoom));

        auto thumbIt = thumbs.find(n.id);
        const uint32_t tex = (thumbIt != thumbs.end()) ? thumbIt->second : 0u;
        if (tex) {
            dl->AddImage(texId(tex), tl, br);
        } else if (n.kind == NodeKind::Output) {
            // The output node has no image of its own; show its accent instead.
            dl->AddRectFilled(tl, br, col(44, 40, 36));
            const char* t  = "OUT";
            const ImVec2 ts = zoomedTextSize(t, st.zoom);
            drawZoomedText(dl, ImVec2(tl.x + (s - ts.x) * 0.5f, tl.y + (s - ts.y) * 0.5f),
                           accent, t, st.zoom);
        }

        // A 3px accent strip along the top is enough to read role at a glance
        // without the title bar eating thumbnail space.
        dl->AddRectFilled(tl, ImVec2(br.x, tl.y + std::max(2.0f, 3.0f * st.zoom)), accent);

        dl->AddRect(tl, br, sel ? col(226, 196, 120)
                                : (inView ? col(120, 180, 240) : col(16, 17, 19)),
                    0.0f, 0, sel ? std::max(1.5f, 2.0f * st.zoom) : 1.2f);

        // label, centered under the thumbnail
        if (lblH > 7.0f) {
            const ImVec2 ts = zoomedTextSize(n.label.c_str(), st.zoom);
            drawZoomedText(dl, ImVec2(tl.x + (s - ts.x) * 0.5f, br.y + 2.0f * st.zoom),
                           sel ? col(238, 220, 170) : col(190, 194, 202),
                           n.label.c_str(), st.zoom);
        }

        const int slots = slotCount(n);
        for (int i = 0; i < slots; ++i) {
            const ImVec2 pin = inPinPos(n, origin, st, i);
            const bool connected = n.inputs[static_cast<size_t>(i)].src != kInvalidNode;
            dl->AddCircleFilled(pin, pr,
                                connected ? col(226, 196, 120) : col(120, 124, 132));
            dl->AddCircle(pin, pr, col(16, 17, 19), 0, 1.4f);
        }

        if (n.kind != NodeKind::Output) {
            const ImVec2 op = outPinPos(n, origin, st);
            dl->AddCircleFilled(op, pr, col(206, 210, 218));
            dl->AddCircle(op, pr, col(16, 17, 19), 0, 1.4f);
        }
    }

    dl->PopClipRect();

    // ---- overlays ----------------------------------------------------------
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f%%", st.zoom * 100.0f);
        dl->AddText(ImVec2(origin.x + 8.0f, origin.y + 6.0f), col(120, 124, 132), buf);
    }

    if (ImGui::BeginPopupContextItem("canvas_ctx")) {
        const ImVec2 at = ImGui::GetMousePosOnOpeningCurrentPopup();
        const float  gx = (at.x - origin.x - st.panX) / st.zoom;
        const float  gy = (at.y - origin.y - st.panY) / st.zoom;

        ImGui::TextDisabled("Add node");
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(Atomic::Count); ++i) {
            const Atomic a = static_cast<Atomic>(i);
            if (ImGui::MenuItem(atomicName(a))) st.selected = g.addAtomic(a, gx, gy);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset view")) {
            st.zoom = 1.0f;
            st.panX = 0.0f;
            st.panY = 0.0f;
        }
        if (st.selected != kInvalidNode && st.selected != g.outputNode()) {
            if (ImGui::MenuItem("Delete selected")) {
                g.remove(st.selected);
                st.selected = kInvalidNode;
            }
        }
        ImGui::EndPopup();
    }

    if (st.selected != kInvalidNode && st.selected != g.outputNode() &&
        ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        g.remove(st.selected);
        st.selected = kInvalidNode;
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Inspector
// ---------------------------------------------------------------------------
void drawParamPanel(Graph& g, CanvasState& st, const PanelContext& ctx) {
    Node* n = g.find(st.selected);
    if (!n) {
        ImGui::TextDisabled("No node selected.");
        ImGui::Spacing();
        ImGui::TextWrapped("Right-click to add a node. Drag from a node's right "
                           "pin onto a left pin to connect; click a left pin to "
                           "detach. Middle-drag pans, the wheel zooms, and "
                           "double-clicking a node opens it in the 2D view.");
        return;
    }

    ImGui::TextUnformatted(n->label.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    const char* kSizeLabels[] = {"64", "128", "256", "512", "1024", "2048", "4096"};

    if (n->kind == NodeKind::Output) {
        int sizeIdx = n->sizeLog2 - 6;
        if (ImGui::Combo("Resolution", &sizeIdx, kSizeLabels, IM_ARRAYSIZE(kSizeLabels))) {
            n->sizeLog2 = sizeIdx + 6;
            g.dirty     = true;
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Channels");
        ImGui::Separator();
        for (int c = 0; c < kPbrChannelCount; ++c) {
            const bool on = n->inputs[static_cast<size_t>(c)].src != kInvalidNode;
            ImGui::TextColored(on ? ImVec4(0.89f, 0.77f, 0.47f, 1.0f)
                                  : ImVec4(0.47f, 0.49f, 0.52f, 1.0f),
                               "%s  %s", on ? "[x]" : "[ ]",
                               pbrChannelName(static_cast<PbrChannel>(c)));
        }
        return;
    }

    // Levels gets a bespoke editor; everything else falls back to generated
    // widgets driven by its ParamDecl list.
    if (n->atomic == Atomic::Levels) {
        if (drawLevelsEditor(g, *n, st, ctx)) g.dirty = true;
    } else {
        const AtomicDef& def = atomicDef(n->atomic);
        for (const auto& p : def.params) {
            Value v       = g.param(*n, p.name);
            bool  changed = false;

            switch (p.type) {
                case ValueType::Float: {
                    float f = asFloat(v);
                    if (ImGui::SliderFloat(p.label.c_str(), &f, p.lo, p.hi)) {
                        v = f; changed = true;
                    }
                    break;
                }
                case ValueType::Int: {
                    int i = asInt(v);
                    if (ImGui::SliderInt(p.label.c_str(), &i, static_cast<int>(p.lo),
                                         static_cast<int>(p.hi))) {
                        v = i; changed = true;
                    }
                    break;
                }
                case ValueType::Bool: {
                    bool b = asBool(v);
                    if (ImGui::Checkbox(p.label.c_str(), &b)) { v = b; changed = true; }
                    break;
                }
                case ValueType::Enum: {
                    int cur = asInt(v);
                    std::vector<const char*> items;
                    items.reserve(p.enumLabels.size());
                    for (const auto& e : p.enumLabels) items.push_back(e.c_str());
                    if (!items.empty() &&
                        ImGui::Combo(p.label.c_str(), &cur, items.data(),
                                     static_cast<int>(items.size()))) {
                        v = cur; changed = true;
                    }
                    break;
                }
                case ValueType::Color: {
                    Rgba  cc     = asRgba(v);
                    float arr[4] = {cc.r, cc.g, cc.b, cc.a};
                    if (ImGui::ColorEdit4(p.label.c_str(), arr)) {
                        v = Rgba{arr[0], arr[1], arr[2], arr[3]};
                        changed = true;
                    }
                    break;
                }
            }

            if (changed) {
                n->params[p.name] = v;
                g.dirty           = true;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    int sizeIdx = n->sizeLog2 - 6;
    if (ImGui::Combo("Output Size", &sizeIdx, kSizeLabels, IM_ARRAYSIZE(kSizeLabels))) {
        n->sizeLog2 = sizeIdx + 6;
        g.dirty     = true;
    }
}

// ---------------------------------------------------------------------------
// 2D viewer
// ---------------------------------------------------------------------------
void drawTextureViewer(uint32_t tex, int texSize, const char* label) {
    if (!tex) {
        ImGui::TextDisabled("Double-click a node in the graph to inspect it here.");
        return;
    }

    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::TextDisabled("%dx%d", texSize, texSize);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float side = std::max(16.0f, std::min(avail.x, avail.y));

    // Centre the square inside whatever shape the panel has.
    const ImVec2 cur = ImGui::GetCursorScreenPos();
    const ImVec2 tl(cur.x + (avail.x - side) * 0.5f, cur.y + (avail.y - side) * 0.5f);
    const ImVec2 br(tl.x + side, tl.y + side);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawCheckerboard(dl, tl, br, 12.0f);
    dl->AddImage(texId(tex), tl, br);
    dl->AddRect(tl, br, col(16, 17, 19));
}

}  // namespace sd
