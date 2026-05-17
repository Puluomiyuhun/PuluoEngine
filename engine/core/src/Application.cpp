#include "puluo/core/Application.h"
#include "puluo/core/Base.h"
#include "puluo/core/Log.h"
#include "puluo/core/Event.h"

#include <chrono>

namespace Puluo {

Application* Application::s_Instance = nullptr;

static float GetTime() {
    static auto start = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<float>(now - start).count();
}

Application::Application(const ApplicationSpec& spec)
    : m_Spec(spec)
{
    PULUO_CORE_ASSERT(!s_Instance, "Application already exists!");
    s_Instance = this;

    WindowProps props;
    props.title  = spec.name;
    props.width  = spec.width;
    props.height = spec.height;
    m_Window = Window::Create(props);

    m_Window->SetEventCallback([this](Event& e) { OnEventInternal(e); });
}

Application::~Application() {
    s_Instance = nullptr;
}

void Application::Run() {
    OnInit();

    while (m_Running) {
        float time = GetTime();
        float deltaTime = time - m_LastFrameTime;
        m_LastFrameTime = time;

        m_Window->OnUpdate();

        if (m_Window->ShouldClose()) {
            m_Running = false;
        }

        OnUpdate(deltaTime);
        OnRender();

        m_Window->SwapBuffers();
    }

    OnShutdown();
}

void Application::OnEventInternal(Event& event) {
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) { return OnWindowClose(e); });
    dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) { return OnWindowResize(e); });

    // Forward to user
    OnEvent(event);
}

bool Application::OnWindowClose(WindowCloseEvent& event) {
    m_Running = false;
    return true;
}

bool Application::OnWindowResize(WindowResizeEvent& event) {
    // Could handle minimization here
    return false;
}

} // namespace Puluo
