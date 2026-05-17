#pragma once

struct GLFWwindow;

namespace Puluo {

enum class EditorTheme {
    DarkSlate = 0,   // UE5-inspired dark cool theme
    PinkCute,        // Soft pink kawaii theme
    Count
};

class ImGuiLayer {
public:
    static void Init(GLFWwindow* window);
    static void Shutdown();
    static void BeginFrame();
    static void EndFrame();
    static bool WantCaptureMouse();
    static bool WantCaptureKeyboard();

    static void        ApplyTheme(EditorTheme theme);
    static EditorTheme GetCurrentTheme();
    static const char* GetThemeName(EditorTheme theme);

private:
    static void ApplyCommonLayout();
    static void ApplyDarkSlateTheme();
    static void ApplyPinkCuteTheme();
    static void DrawCuteBackground();

    static inline EditorTheme s_CurrentTheme = EditorTheme::DarkSlate;
};

} // namespace Puluo
