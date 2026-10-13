// PuluoDesigner -- a node-based material authoring tool.
//
// Runs as its own executable and links PuluoEngine purely as a consumer: the
// engine is unmodified and unaware of this app. The only contact surface is
// PreviewScene, which borrows the PBR pipeline to shade a sphere with whatever
// the graph currently evaluates to.
//
// Because the node editor and the 3D view share one process and one GL
// context, graph output textures are handed to the renderer by name -- no
// export to disk, no copy, no second process.

#include "puluo/core/Application.h"
#include "puluo/core/EntryPoint.h"
#include "puluo/core/Log.h"
#include "puluo/platform/ImGuiLayer.h"
#include "puluo/renderer/Renderer.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

#include "graph/Evaluator.h"
#include "graph/Graph.h"
#include "preview/PreviewScene.h"
#include "ui/Ui.h"

namespace {
constexpr const char* kAssetDir  = "assets";
constexpr const char* kShaderDir = "assets/shaders/texgraph";
}  // namespace

class DesignerApp : public Puluo::Application {
public:
    using Puluo::Application::Application;

    void OnInit() override {
        Puluo::Renderer::Init();

        auto* glfwWindow = static_cast<GLFWwindow*>(GetWindow().GetNativeWindow());
        Puluo::ImGuiLayer::Init(glfwWindow);
        sd::applySdTheme();

        if (!m_Eval.init(kShaderDir))
            PULUO_CORE_ERROR("[designer] some atomics failed to build; "
                             "those nodes will evaluate to nothing");

        if (!m_Hist.init(kShaderDir))
            PULUO_CORE_WARN("[designer] histogram unavailable; the Levels "
                            "editor will fall back to its slider interface");

        if (!m_Preview.init(kAssetDir))
            PULUO_CORE_ERROR("[designer] preview scene unavailable");

        BuildDefaultGraph();
    }

    void OnShutdown() override {
        m_Preview.shutdown();
        m_Hist.shutdown();
        m_Eval.shutdown();
        Puluo::ImGuiLayer::Shutdown();
        Puluo::Renderer::Shutdown();
    }

    void OnRender() override {
        // Re-evaluate only when the graph actually changed. Parameter drags
        // mark it dirty every frame, which is exactly when we want the
        // recompute, but an idle graph costs nothing.
        bool reevaluated = false;
        if (m_Graph.dirty) {
            m_Maps        = m_Eval.evaluate(m_Graph);
            m_Graph.dirty = false;
            reevaluated   = true;
        }

        const sd::PanelContext ctx = UpdateHistogram(reevaluated);

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(
            static_cast<GLFWwindow*>(GetWindow().GetNativeWindow()), &fbW, &fbH);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.07f, 0.07f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Puluo::ImGuiLayer::BeginFrame();
        DrawDockSpace();
        DrawGraphPanel();
        DrawPreviewPanel();
        Draw2DViewPanel();
        DrawPropertiesPanel(ctx);
        ImGui::End();  // DockSpace
        Puluo::ImGuiLayer::EndFrame();
    }

private:
    sd::Graph             m_Graph;
    sd::CanvasState       m_Canvas;
    sd::Evaluator         m_Eval;
    sd::EvalResult        m_Maps;
    sd::PreviewScene      m_Preview;
    sd::HistogramComputer m_Hist;
    bool                  m_FirstFrame = true;

    // Histogram cache: recomputed only when the graph re-evaluates or the
    // inspected node changes, not every frame.
    sd::NodeId m_HistFor = sd::kInvalidNode;
    uint32_t   m_HistBins[sd::HistogramComputer::kBins] = {};
    uint32_t   m_HistMax = 0;

    void BuildDefaultGraph() {
        // The smallest meaningful graph: one generator wired to Base Color.
        const sd::NodeId color = m_Graph.addAtomic(sd::Atomic::UniformColor, 60.0f, 120.0f);
        if (sd::Node* n = m_Graph.find(color))
            n->params["uColor"] = sd::Rgba{0.72f, 0.38f, 0.26f, 1.0f};

        m_Graph.connect(color, m_Graph.outputNode(),
                        static_cast<int>(sd::PbrChannel::BaseColor));

        if (sd::Node* out = m_Graph.find(m_Graph.outputNode())) {
            out->uiX = 320.0f;
            out->uiY = 120.0f;
        }

        m_Canvas.selected    = color;
        m_Canvas.previewNode = color;
        m_Graph.dirty        = true;
    }

    // Returns the context the inspector needs, refreshing the cached histogram
    // of the selected Levels node's input when necessary.
    sd::PanelContext UpdateHistogram(bool reevaluated) {
        sd::PanelContext ctx;

        const sd::Node* sel = m_Graph.find(m_Canvas.selected);
        if (!sel || sel->kind != sd::NodeKind::Atomic ||
            sel->atomic != sd::Atomic::Levels) {
            m_HistFor = sd::kInvalidNode;
            return ctx;
        }

        const sd::NodeId srcId  = sel->inputs.empty() ? sd::kInvalidNode
                                                      : sel->inputs[0].src;
        const uint32_t   srcTex = m_Eval.nodeTexture(srcId);
        if (!srcTex) {
            m_HistFor = sd::kInvalidNode;
            return ctx;
        }

        if (reevaluated || m_HistFor != sel->id) {
            const sd::Node* srcNode = m_Graph.find(srcId);
            const int size = srcNode ? (1 << srcNode->sizeLog2) : 0;
            if (size > 0 && m_Hist.compute(srcTex, size, m_HistBins)) {
                m_HistMax = *std::max_element(
                    m_HistBins, m_HistBins + sd::HistogramComputer::kBins);
                m_HistFor = sel->id;
            } else {
                m_HistFor = sd::kInvalidNode;
            }
        }

        if (m_HistFor == sel->id) {
            ctx.histBins = m_HistBins;
            ctx.histMax  = m_HistMax;
        }
        return ctx;
    }

    void DrawDockSpace() {
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::SetNextWindowViewport(vp->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DesignerDockSpace", nullptr, flags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockId = ImGui::GetID("DesignerDock");
        ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        if (m_FirstFrame) {
            m_FirstFrame = false;
            ImGui::DockBuilderRemoveNode(dockId);
            ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);

            // Left: graph. Right column, top to bottom: 3D view, 2D view,
            // properties -- the arrangement SD defaults to.
            ImGuiID right = 0;
            const ImGuiID left =
                ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, 0.56f, nullptr, &right);
            ImGuiID rightRest = 0;
            const ImGuiID rightTop =
                ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.38f, nullptr, &rightRest);
            ImGuiID rightBottom = 0;
            const ImGuiID rightMid =
                ImGui::DockBuilderSplitNode(rightRest, ImGuiDir_Up, 0.45f, nullptr,
                                            &rightBottom);

            ImGui::DockBuilderDockWindow("Graph", left);
            ImGui::DockBuilderDockWindow("3D View", rightTop);
            ImGui::DockBuilderDockWindow("2D View", rightMid);
            ImGui::DockBuilderDockWindow("Properties", rightBottom);
            ImGui::DockBuilderFinish(dockId);
        }

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) Close();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Skybox", nullptr, &m_Preview.showSkybox);
                if (ImGui::MenuItem("Reset graph view")) {
                    m_Canvas.zoom = 1.0f;
                    m_Canvas.panX = 0.0f;
                    m_Canvas.panY = 0.0f;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }

    void DrawGraphPanel() {
        ImGui::Begin("Graph");
        sd::drawNodeCanvas("graph_canvas", m_Graph, m_Canvas, m_Eval.nodeTextures());
        ImGui::End();
    }

    void DrawPreviewPanel() {
        ImGui::Begin("3D View");

        ImVec2 size = ImGui::GetContentRegionAvail();
        const int w = static_cast<int>(size.x);
        const int h = static_cast<int>(size.y);

        if (w > 0 && h > 0) {
            m_Preview.render(m_Maps, w, h);

            const uint32_t tex = m_Preview.colorTexture();
            if (tex) {
                ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(tex)),
                             size, ImVec2(0, 1), ImVec2(1, 0));

                // Orbit only while the image itself is under the cursor, so the
                // panel's docking and scrollbars still behave normally.
                if (ImGui::IsItemHovered()) {
                    ImGuiIO& io = ImGui::GetIO();
                    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                        m_Preview.orbit(io.MouseDelta.x * 0.4f, -io.MouseDelta.y * 0.4f);
                    if (io.MouseWheel != 0.0f)
                        m_Preview.zoom(io.MouseWheel * 0.3f);
                }
            }
        }

        ImGui::End();
    }

    void Draw2DViewPanel() {
        ImGui::Begin("2D View");

        uint32_t    tex   = 0;
        int         size  = 0;
        const char* label = "";

        if (const sd::Node* n = m_Graph.find(m_Canvas.previewNode)) {
            label = n->label.c_str();
            size  = 1 << n->sizeLog2;
            // The output node owns no texture of its own, so show the channel
            // it ultimately produces.
            tex = (n->kind == sd::NodeKind::Output)
                      ? m_Maps.channel[static_cast<int>(sd::PbrChannel::BaseColor)]
                      : m_Eval.nodeTexture(n->id);
        }

        sd::drawTextureViewer(tex, size, label);
        ImGui::End();
    }

    void DrawPropertiesPanel(const sd::PanelContext& ctx) {
        ImGui::Begin("Properties");
        sd::drawParamPanel(m_Graph, m_Canvas, ctx);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Preview fallbacks");
        ImGui::SliderFloat("Metallic##fb", &m_Preview.fallbackMetallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness##fb", &m_Preview.fallbackRoughness, 0.0f, 1.0f);
        ImGui::End();
    }
};

Puluo::Application* Puluo::CreateApplication() {
    Puluo::ApplicationSpec spec;
    spec.name   = "PuluoDesigner";
    spec.width  = 1600;
    spec.height = 900;
    return new DesignerApp(spec);
}
