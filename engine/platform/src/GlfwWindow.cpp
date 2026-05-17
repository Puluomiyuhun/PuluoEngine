#include "puluo/core/Window.h"
#include "puluo/core/Log.h"
#include "puluo/core/Base.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

namespace Puluo {

static bool s_GLFWInitialized = false;

static void GLFWErrorCallback(int error, const char* description) {
    PULUO_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
}

class GlfwWindow : public Window {
public:
    GlfwWindow(const WindowProps& props) {
        m_Data.title  = props.title;
        m_Data.width  = props.width;
        m_Data.height = props.height;

        PULUO_CORE_INFO("Creating window: {0} ({1}x{2})", props.title, props.width, props.height);

        if (!s_GLFWInitialized) {
            int success = glfwInit();
            PULUO_CORE_ASSERT(success, "Failed to initialize GLFW!");
            glfwSetErrorCallback(GLFWErrorCallback);
            s_GLFWInitialized = true;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_Window = glfwCreateWindow(props.width, props.height, props.title.c_str(), nullptr, nullptr);
        PULUO_CORE_ASSERT(m_Window, "Failed to create GLFW window!");

        glfwMaximizeWindow(m_Window);

        glfwMakeContextCurrent(m_Window);

        int version = gladLoadGL(glfwGetProcAddress);
        PULUO_CORE_ASSERT(version, "Failed to initialize glad!");
        PULUO_CORE_INFO("OpenGL {0}.{1} loaded", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

        glfwSwapInterval(1);
        glfwSetWindowUserPointer(m_Window, &m_Data);

        // ---- Register GLFW callbacks ----

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            WindowCloseEvent event;
            if (data.eventCallback) data.eventCallback(event);
        });

        glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            data.width = width;
            data.height = height;
            glViewport(0, 0, width, height);
            WindowResizeEvent event(width, height);
            if (data.eventCallback) data.eventCallback(event);
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if (!data.eventCallback) return;

            switch (action) {
                case GLFW_PRESS: {
                    KeyPressedEvent event(static_cast<Key>(key), false);
                    data.eventCallback(event);
                    break;
                }
                case GLFW_RELEASE: {
                    KeyReleasedEvent event(static_cast<Key>(key));
                    data.eventCallback(event);
                    break;
                }
                case GLFW_REPEAT: {
                    KeyPressedEvent event(static_cast<Key>(key), true);
                    data.eventCallback(event);
                    break;
                }
            }
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int /*mods*/) {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if (!data.eventCallback) return;

            switch (action) {
                case GLFW_PRESS: {
                    MouseButtonPressedEvent event(static_cast<MouseButton>(button));
                    data.eventCallback(event);
                    break;
                }
                case GLFW_RELEASE: {
                    MouseButtonReleasedEvent event(static_cast<MouseButton>(button));
                    data.eventCallback(event);
                    break;
                }
            }
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos) {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            MouseMovedEvent event(static_cast<float>(xPos), static_cast<float>(yPos));
            if (data.eventCallback) data.eventCallback(event);
        });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset) {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            MouseScrolledEvent event(static_cast<float>(xOffset), static_cast<float>(yOffset));
            if (data.eventCallback) data.eventCallback(event);
        });
    }

    ~GlfwWindow() override {
        glfwDestroyWindow(m_Window);
        glfwTerminate();
        s_GLFWInitialized = false;
    }

    void OnUpdate() override {
        glfwPollEvents();
    }

    void SwapBuffers() override {
        glfwSwapBuffers(m_Window);
    }

    int GetWidth() const override { return m_Data.width; }
    int GetHeight() const override { return m_Data.height; }
    bool ShouldClose() const override { return glfwWindowShouldClose(m_Window); }

    void SetEventCallback(const EventCallbackFn& callback) override {
        m_Data.eventCallback = callback;
    }

    void SetCursorMode(bool enabled) override {
        glfwSetInputMode(m_Window, GLFW_CURSOR, enabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

    void* GetNativeWindow() const override { return m_Window; }

private:
    struct WindowData {
        std::string title;
        int width = 0;
        int height = 0;
        EventCallbackFn eventCallback;
    };

    GLFWwindow* m_Window = nullptr;
    WindowData m_Data;
};

std::unique_ptr<Window> Window::Create(const WindowProps& props) {
    return std::make_unique<GlfwWindow>(props);
}

} // namespace Puluo
