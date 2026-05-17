#include "puluo/platform/ImGuiLayer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <cmath>

namespace Puluo {

// ---- Helper: draw a tiny heart at center (cx, cy) with given size ----
static void DrawHeart(ImDrawList* dl, float cx, float cy, float size, ImU32 col) {
    // Heart as two arcs + a triangle bottom
    const float r = size * 0.30f;
    const float topY = cy - size * 0.15f;
    // Left and right circle centers
    float lx = cx - r, rx = cx + r;
    dl->AddCircleFilled(ImVec2(lx, topY), r, col, 16);
    dl->AddCircleFilled(ImVec2(rx, topY), r, col, 16);
    // Triangle pointing down
    ImVec2 triPts[3] = {
        ImVec2(cx - size * 0.58f, topY + r * 0.15f),
        ImVec2(cx + size * 0.58f, topY + r * 0.15f),
        ImVec2(cx, cy + size * 0.55f)
    };
    dl->AddTriangleFilled(triPts[0], triPts[1], triPts[2], col);
}

// ---- Helper: draw a tiny star at center (cx, cy) ----
static void DrawStar(ImDrawList* dl, float cx, float cy, float size, ImU32 col) {
    const float outer = size * 0.45f;
    const float inner = size * 0.20f;
    ImVec2 pts[10];
    for (int i = 0; i < 10; i++) {
        float angle = (float)i * 3.14159265f / 5.0f - 3.14159265f / 2.0f;
        float r = (i % 2 == 0) ? outer : inner;
        pts[i] = ImVec2(cx + cosf(angle) * r, cy + sinf(angle) * r);
    }
    dl->AddConvexPolyFilled(pts, 10, col);
}

// Draw repeating cute pattern on the foreground layer (over window bg, under nothing)
void ImGuiLayer::DrawCuteBackground() {
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    // Pattern colors — soft, semi-transparent overlay
    const ImU32 heartCol = IM_COL32(200, 110, 145, 30);  // rose hearts
    const ImU32 starCol  = IM_COL32(170, 130, 195, 32);  // lavender stars — boosted
    const ImU32 dotCol   = IM_COL32(195, 125, 160, 22);  // pink dots

    const float spacing = 120.0f;
    const float heartSize = 24.0f;
    const float starSize  = 20.0f;
    const float dotRadius = 4.0f;

    // Slow drift animation based on time
    double time = glfwGetTime();
    float offsetX = fmodf((float)time * 3.0f, spacing);
    float offsetY = fmodf((float)time * 2.0f, spacing);

    int row = 0;
    for (float y = -spacing + offsetY; y < displaySize.y + spacing; y += spacing) {
        int col = 0;
        float rowShift = (row % 2) ? spacing * 0.5f : 0.0f;
        for (float x = -spacing + offsetX + rowShift; x < displaySize.x + spacing; x += spacing) {
            int pattern = (row + col) % 3;
            if (pattern == 0)
                DrawHeart(fg, x, y, heartSize, heartCol);
            else if (pattern == 1)
                DrawStar(fg, x, y + 2.0f, starSize, starCol);
            else
                fg->AddCircleFilled(ImVec2(x, y), dotRadius, dotCol, 8);
            col++;
        }
        row++;
    }
}

// ---- Shared layout parameters (same for all themes) ----
void ImGuiLayer::ApplyCommonLayout() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Window / Frame shape
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(5.0f, 4.0f);
    style.ItemSpacing       = ImVec2(6.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
    style.IndentSpacing     = 16.0f;
    style.ScrollbarSize     = 14.0f;
    style.GrabMinSize       = 10.0f;

    // Borders
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;
}

// ===========================================================================
// Dark Slate Theme  (UE5-inspired)
// ===========================================================================
void ImGuiLayer::ApplyDarkSlateTheme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ApplyCommonLayout();

    // Rounding
    style.WindowRounding    = 2.0f;
    style.ChildRounding     = 2.0f;
    style.FrameRounding     = 2.0f;
    style.PopupRounding     = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding      = 2.0f;
    style.TabRounding       = 2.0f;

    ImVec4* c = style.Colors;

    // Backgrounds
    c[ImGuiCol_WindowBg]             = ImVec4(0.118f, 0.118f, 0.118f, 1.00f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.118f, 0.118f, 0.118f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.137f, 0.137f, 0.137f, 0.98f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.157f, 0.157f, 0.157f, 1.00f);

    // Title bar
    c[ImGuiCol_TitleBg]              = ImVec4(0.098f, 0.098f, 0.098f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.118f, 0.118f, 0.118f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.098f, 0.098f, 0.098f, 0.80f);

    // Tabs
    c[ImGuiCol_Tab]                  = ImVec4(0.137f, 0.137f, 0.137f, 1.00f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.235f, 0.235f, 0.235f, 1.00f);
    c[ImGuiCol_TabSelected]          = ImVec4(0.176f, 0.176f, 0.176f, 1.00f);
    c[ImGuiCol_TabDimmed]            = ImVec4(0.098f, 0.098f, 0.098f, 1.00f);
    c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.137f, 0.137f, 0.137f, 1.00f);

    // Frame / Input fields
    c[ImGuiCol_FrameBg]              = ImVec4(0.078f, 0.078f, 0.078f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.137f, 0.137f, 0.137f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.157f, 0.157f, 0.157f, 1.00f);

    // Headers
    c[ImGuiCol_Header]               = ImVec4(0.176f, 0.176f, 0.176f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.235f, 0.235f, 0.235f, 1.00f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.196f, 0.196f, 0.196f, 1.00f);

    // Buttons
    c[ImGuiCol_Button]               = ImVec4(0.176f, 0.176f, 0.176f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.255f, 0.255f, 0.255f, 1.00f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.200f, 0.200f, 0.200f, 1.00f);

    // Accent — UE blue
    c[ImGuiCol_SliderGrab]           = ImVec4(0.200f, 0.392f, 0.722f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.255f, 0.471f, 0.843f, 1.00f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.200f, 0.392f, 0.722f, 1.00f);

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.098f, 0.098f, 0.098f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.235f, 0.235f, 0.235f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.314f, 0.314f, 0.314f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.392f, 0.392f, 0.392f, 1.00f);

    // Separator / Border / Resize grip
    c[ImGuiCol_Separator]            = ImVec4(0.196f, 0.196f, 0.196f, 1.00f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.314f, 0.314f, 0.314f, 1.00f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.200f, 0.392f, 0.722f, 1.00f);
    c[ImGuiCol_Border]               = ImVec4(0.157f, 0.157f, 0.157f, 1.00f);
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.200f, 0.392f, 0.722f, 0.25f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.200f, 0.392f, 0.722f, 0.67f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.200f, 0.392f, 0.722f, 0.95f);

    // Docking
    c[ImGuiCol_DockingPreview]       = ImVec4(0.200f, 0.392f, 0.722f, 0.70f);
    c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.078f, 0.078f, 0.078f, 1.00f);

    // Text
    c[ImGuiCol_Text]                 = ImVec4(0.863f, 0.863f, 0.863f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.431f, 0.431f, 0.431f, 1.00f);
}

// ===========================================================================
// Pink Cute Theme  (soft, warm, kawaii)
// ===========================================================================
void ImGuiLayer::ApplyPinkCuteTheme() {
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    ApplyCommonLayout();

    // Extra round & pillowy
    style.WindowRounding    = 10.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 10.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding      = 10.0f;
    style.TabRounding       = 8.0f;

    // Generous padding for the cute feel
    style.FramePadding      = ImVec2(7.0f, 5.0f);
    style.WindowPadding     = ImVec2(10.0f, 10.0f);
    style.FrameBorderSize   = 1.0f;

    ImVec4* c = style.Colors;

    // ---- Palette (warm pink — saturated but not harsh) ----
    const ImVec4 pinkMist     (0.878f, 0.784f, 0.820f, 0.88f); // main bg — semi-transparent so pattern shows
    const ImVec4 blush        (0.851f, 0.737f, 0.792f, 1.00f); // #D9BCC9  panel/tab bg — pinker
    const ImVec4 petalPink    (0.820f, 0.671f, 0.745f, 1.00f); // #D1ABBE  headers
    const ImVec4 rose         (0.780f, 0.545f, 0.647f, 1.00f); // #C78BA5  buttons, scrollbar
    const ImVec4 hotPink      (0.820f, 0.380f, 0.530f, 1.00f); // #D16187  accent hover
    const ImVec4 deepRose     (0.722f, 0.275f, 0.420f, 1.00f); // #B8466B  accent active
    const ImVec4 lavender     (0.765f, 0.690f, 0.830f, 1.00f); // #C3B0D4  nav highlight
    const ImVec4 roseWhite    (0.918f, 0.843f, 0.878f, 0.92f); // input fields, popup bg — slightly transparent
    const ImVec4 warmGray     (0.520f, 0.420f, 0.470f, 1.00f); // text-muted
    const ImVec4 darkText     (0.280f, 0.180f, 0.230f, 1.00f); // #472E3B  readable dark rose-brown

    // Backgrounds — warm pink, slightly transparent to let pattern through
    c[ImGuiCol_WindowBg]             = pinkMist;
    c[ImGuiCol_ChildBg]              = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_PopupBg]              = roseWhite;
    c[ImGuiCol_MenuBarBg]            = ImVec4(blush.x, blush.y, blush.z, 0.92f);

    // Title bar
    c[ImGuiCol_TitleBg]              = blush;
    c[ImGuiCol_TitleBgActive]        = petalPink;
    c[ImGuiCol_TitleBgCollapsed]     = blush;

    // Tabs
    c[ImGuiCol_Tab]                  = blush;
    c[ImGuiCol_TabHovered]           = rose;
    c[ImGuiCol_TabSelected]          = petalPink;
    c[ImGuiCol_TabDimmed]            = blush;
    c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.863f, 0.769f, 0.816f, 1.00f);

    // Frame / Input fields — roseWhite instead of pure white
    c[ImGuiCol_FrameBg]              = roseWhite;
    c[ImGuiCol_FrameBgHovered]       = blush;
    c[ImGuiCol_FrameBgActive]        = petalPink;

    // Headers
    c[ImGuiCol_Header]               = blush;
    c[ImGuiCol_HeaderHovered]        = rose;
    c[ImGuiCol_HeaderActive]         = petalPink;

    // Buttons — rosy pill buttons
    c[ImGuiCol_Button]               = rose;
    c[ImGuiCol_ButtonHovered]        = hotPink;
    c[ImGuiCol_ButtonActive]         = deepRose;

    // Accent
    c[ImGuiCol_SliderGrab]           = hotPink;
    c[ImGuiCol_SliderGrabActive]     = deepRose;
    c[ImGuiCol_CheckMark]            = deepRose;

    // Scrollbar — cute rounded candy bar (bg transparent for pattern)
    c[ImGuiCol_ScrollbarBg]          = ImVec4(pinkMist.x, pinkMist.y, pinkMist.z, 0.50f);
    c[ImGuiCol_ScrollbarGrab]        = rose;
    c[ImGuiCol_ScrollbarGrabHovered] = hotPink;
    c[ImGuiCol_ScrollbarGrabActive]  = deepRose;

    // Separator / Border — soft pink borders
    c[ImGuiCol_Separator]            = petalPink;
    c[ImGuiCol_SeparatorHovered]     = rose;
    c[ImGuiCol_SeparatorActive]      = hotPink;
    c[ImGuiCol_Border]               = ImVec4(petalPink.x, petalPink.y, petalPink.z, 0.70f);
    c[ImGuiCol_ResizeGrip]           = ImVec4(hotPink.x, hotPink.y, hotPink.z, 0.25f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(hotPink.x, hotPink.y, hotPink.z, 0.67f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(hotPink.x, hotPink.y, hotPink.z, 0.95f);

    // Docking
    c[ImGuiCol_DockingPreview]       = ImVec4(hotPink.x, hotPink.y, hotPink.z, 0.70f);
    c[ImGuiCol_DockingEmptyBg]       = ImVec4(pinkMist.x, pinkMist.y, pinkMist.z, 0.60f);

    // Text — warm rose-brown, easy on the eyes
    c[ImGuiCol_Text]                 = darkText;
    c[ImGuiCol_TextDisabled]         = warmGray;

    // Nav highlight — soft lavender
    c[ImGuiCol_NavHighlight]         = lavender;
}

// ===========================================================================
// Public API
// ===========================================================================
void ImGuiLayer::ApplyTheme(EditorTheme theme) {
    switch (theme) {
    case EditorTheme::DarkSlate:  ApplyDarkSlateTheme(); break;
    case EditorTheme::PinkCute:   ApplyPinkCuteTheme();  break;
    default:                      ApplyDarkSlateTheme(); break;
    }
    s_CurrentTheme = theme;
}

EditorTheme ImGuiLayer::GetCurrentTheme() {
    return s_CurrentTheme;
}

const char* ImGuiLayer::GetThemeName(EditorTheme theme) {
    switch (theme) {
    case EditorTheme::DarkSlate: return "Dark Slate";
    case EditorTheme::PinkCute:  return "Pink Cute";
    default:                     return "Unknown";
    }
}

void ImGuiLayer::Init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // ---- Font: Roboto (similar to UE5) ----
    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Medium.ttf", 17.0f);

    // Apply default theme
    ApplyTheme(EditorTheme::DarkSlate);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");
}

void ImGuiLayer::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::EndFrame() {
    // Draw cute pattern overlay before Render() finalizes draw data
    if (s_CurrentTheme == EditorTheme::PinkCute) {
        DrawCuteBackground();
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool ImGuiLayer::WantCaptureMouse() {
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiLayer::WantCaptureKeyboard() {
    return ImGui::GetIO().WantCaptureKeyboard;
}

} // namespace Puluo
