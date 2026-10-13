#include "Ui.h"

#include <imgui.h>

namespace sd {

void applySdTheme() {
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding    = 2.0f;
    s.FrameRounding     = 2.0f;
    s.GrabRounding      = 2.0f;
    s.TabRounding       = 2.0f;
    s.ScrollbarRounding = 2.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.WindowPadding     = ImVec2(6, 6);
    s.FramePadding      = ImVec2(6, 3);
    s.ItemSpacing       = ImVec2(6, 4);
    s.IndentSpacing     = 16.0f;

    auto rgb = [](int r, int g, int b, float a = 1.0f) {
        return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
    };

    // Neutral cool greys with a desaturated blue accent -- the register
    // Substance Designer's default dark theme sits in.
    const ImVec4 bg0     = rgb(30, 31, 34);
    const ImVec4 bg1     = rgb(38, 39, 43);
    const ImVec4 bg2     = rgb(46, 48, 53);
    const ImVec4 bg3     = rgb(56, 58, 64);
    const ImVec4 text    = rgb(214, 216, 220);
    const ImVec4 textDim = rgb(130, 134, 142);
    const ImVec4 accent  = rgb(78, 128, 176);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                 = text;
    c[ImGuiCol_TextDisabled]         = textDim;
    c[ImGuiCol_WindowBg]             = bg0;
    c[ImGuiCol_ChildBg]              = bg0;
    c[ImGuiCol_PopupBg]              = bg1;
    c[ImGuiCol_Border]               = rgb(20, 21, 23);
    c[ImGuiCol_FrameBg]              = bg2;
    c[ImGuiCol_FrameBgHovered]       = bg3;
    c[ImGuiCol_FrameBgActive]        = rgb(64, 67, 74);
    c[ImGuiCol_TitleBg]              = bg1;
    c[ImGuiCol_TitleBgActive]        = bg2;
    c[ImGuiCol_TitleBgCollapsed]     = bg0;
    c[ImGuiCol_MenuBarBg]            = bg1;
    c[ImGuiCol_ScrollbarBg]          = bg0;
    c[ImGuiCol_ScrollbarGrab]        = bg3;
    c[ImGuiCol_ScrollbarGrabHovered] = rgb(72, 75, 82);
    c[ImGuiCol_ScrollbarGrabActive]  = accent;
    c[ImGuiCol_CheckMark]            = accent;
    c[ImGuiCol_SliderGrab]           = accent;
    c[ImGuiCol_SliderGrabActive]     = rgb(98, 148, 196);
    c[ImGuiCol_Button]               = bg2;
    c[ImGuiCol_ButtonHovered]        = bg3;
    c[ImGuiCol_ButtonActive]         = accent;
    c[ImGuiCol_Header]               = bg2;
    c[ImGuiCol_HeaderHovered]        = bg3;
    c[ImGuiCol_HeaderActive]         = accent;
    c[ImGuiCol_Separator]            = rgb(24, 25, 28);
    c[ImGuiCol_SeparatorHovered]     = accent;
    c[ImGuiCol_ResizeGrip]           = bg3;
    c[ImGuiCol_ResizeGripHovered]    = accent;
    c[ImGuiCol_Tab]                  = bg1;
    c[ImGuiCol_TabHovered]           = bg3;
    c[ImGuiCol_TabSelected]          = bg2;
    c[ImGuiCol_TabDimmed]            = bg0;
    c[ImGuiCol_TabDimmedSelected]    = bg1;
    c[ImGuiCol_DockingPreview]       = ImVec4(accent.x, accent.y, accent.z, 0.5f);
    c[ImGuiCol_DockingEmptyBg]       = bg0;
    c[ImGuiCol_PlotLines]            = textDim;
    c[ImGuiCol_PlotHistogram]        = accent;
    c[ImGuiCol_TableHeaderBg]        = bg1;
    c[ImGuiCol_TableBorderStrong]    = rgb(24, 25, 28);
    c[ImGuiCol_TableBorderLight]     = rgb(34, 35, 39);
    c[ImGuiCol_TextSelectedBg]       = ImVec4(accent.x, accent.y, accent.z, 0.4f);
    c[ImGuiCol_NavCursor]            = accent;
}

}  // namespace sd
