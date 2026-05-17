#pragma once

#include "puluo/core/Window.h"

#include <string>
#include <memory>

namespace Puluo {

struct ApplicationSpec {
    std::string name = "PuluoEngine";
    int width  = 1280;
    int height = 720;
};

class Application {
public:
    Application(const ApplicationSpec& spec);
    virtual ~Application();

    void Run();

    virtual void OnInit() {}
    virtual void OnShutdown() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender() {}
    virtual void OnEvent(Event& event) {}

    Window& GetWindow() { return *m_Window; }
    static Application& Get() { return *s_Instance; }

    bool IsRunning() const { return m_Running; }
    void Close() { m_Running = false; }

private:
    void OnEventInternal(Event& event);
    bool OnWindowClose(WindowCloseEvent& event);
    bool OnWindowResize(WindowResizeEvent& event);
    ApplicationSpec m_Spec;
    std::unique_ptr<Window> m_Window;
    bool m_Running = true;
    float m_LastFrameTime = 0.0f;

    static Application* s_Instance;
};

// Defined by user
Application* CreateApplication();

} // namespace Puluo
