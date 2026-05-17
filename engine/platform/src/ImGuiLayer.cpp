#include "puluo/platform/ImGuiLayer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

namespace Puluo {

void ImGuiLayer::Init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // ---- Font: Roboto (similar to UE5) ----
    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Medium.ttf", 15.0f);

    // ---- UE-style dark theme ----
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // Window / Frame shape
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(5.0f, 4.0f);
    style.ItemSpacing       = ImVec2(6.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
    style.IndentSpacing     = 16.0f;
    style.ScrollbarSize     = 14.0f;
    style.GrabMinSize       = 10.0f;

    // Rounding
    style.WindowRounding    = 2.0f;
    style.ChildRounding     = 2.0f;
    style.FrameRounding     = 2.0f;
    style.PopupRounding     = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding      = 2.0f;
    style.TabRounding       = 2.0f;

    // Borders
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;

    // Colors (UE5-inspired dark slate palette)
    ImVec4* c = style.Colors;

    // Backgrounds
    c[ImGuiCol_WindowBg]             = ImVec4(0.118f, 0.118f, 0.118f, 1.00f); // #1E1E1E
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
    c[ImGuiCol_FrameBg]              = ImVec4(0.078f, 0.078f, 0.078f, 1.00f); // dark input bg
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.137f, 0.137f, 0.137f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.157f, 0.157f, 0.157f, 1.00f);

    // Headers (collapsing headers, tree nodes, selectable)
    c[ImGuiCol_Header]               = ImVec4(0.176f, 0.176f, 0.176f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.235f, 0.235f, 0.235f, 1.00f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.196f, 0.196f, 0.196f, 1.00f);

    // Buttons
    c[ImGuiCol_Button]               = ImVec4(0.176f, 0.176f, 0.176f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.255f, 0.255f, 0.255f, 1.00f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.200f, 0.200f, 0.200f, 1.00f);

    // Accent (sliders, checkmarks, etc.) — UE blue accent
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
