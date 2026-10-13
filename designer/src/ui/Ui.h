#pragma once
// Designer UI.
//
// The node canvas is hand-rolled on ImGui draw lists rather than pulling in
// imgui-node-editor: no new dependency to configure, and matching SD's visual
// language is mostly a matter of pixel choices, which is easier when we own the
// drawing code.
//
// This layer stays free of OpenGL and engine headers. Textures arrive as plain
// GL names and histogram data as a plain array, so nothing here needs to know
// how either was produced.

#include <cstdint>
#include <map>

#include "../graph/Graph.h"

namespace sd {

// Applies an SD-like dark palette on top of whatever theme the engine set.
void applySdTheme();

enum class DragMode { None, Pan, MoveNode, NewLink };

struct CanvasState {
    float panX = 0.0f;
    float panY = 0.0f;
    float zoom = 1.0f;

    NodeId selected = kInvalidNode;
    // Node shown in the 2D viewer; set by double-clicking a node, the way SD's
    // 2D view follows the node you open.
    NodeId previewNode = kInvalidNode;

    DragMode mode     = DragMode::None;
    NodeId   dragNode = kInvalidNode;
    float    grabDX   = 0.0f;
    float    grabDY   = 0.0f;

    // In-progress link, dragged from a node's output pin.
    NodeId linkSrc = kInvalidNode;

    // Levels editor: index of the handle being dragged, -1 when idle.
    int  levelsDrag    = -1;
    bool levelsSliders = false;  // SD's histogram / sliders interface toggle
};

// Extra data the inspector needs but must not compute itself.
struct PanelContext {
    const uint32_t* histBins = nullptr;  // 256 entries, or null when absent
    uint32_t        histMax  = 0;
};

// Draws the graph canvas and applies any edits directly to `g`.
// `thumbs` maps node id -> GL texture name for the per-node preview images.
void drawNodeCanvas(const char* strId, Graph& g, CanvasState& st,
                    const std::map<NodeId, uint32_t>& thumbs);

// Draws the parameter inspector for the current selection.
void drawParamPanel(Graph& g, CanvasState& st, const PanelContext& ctx);

// Draws a fit-to-panel 2D view of one texture, with its label.
void drawTextureViewer(uint32_t tex, int texSize, const char* label);

}  // namespace sd
