#pragma once

struct GLFWwindow;

namespace Puluo {

class ImGuiLayer {
public:
    static void Init(GLFWwindow* window);
    static void Shutdown();
    static void BeginFrame();
    static void EndFrame();
    static bool WantCaptureMouse();
    static bool WantCaptureKeyboard();
};

} // namespace Puluo
