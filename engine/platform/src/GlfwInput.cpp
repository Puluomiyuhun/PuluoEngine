#include "puluo/core/Input.h"
#include "puluo/core/Application.h"

#include <GLFW/glfw3.h>

namespace Puluo {

static GLFWwindow* GetGLFWWindow() {
    return static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
}

bool Input::IsKeyPressed(Key key) {
    int state = glfwGetKey(GetGLFWWindow(), static_cast<int>(key));
    return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::IsMouseButtonPressed(MouseButton button) {
    int state = glfwGetMouseButton(GetGLFWWindow(), static_cast<int>(button));
    return state == GLFW_PRESS;
}

Vec2 Input::GetMousePosition() {
    double x, y;
    glfwGetCursorPos(GetGLFWWindow(), &x, &y);
    return {static_cast<float>(x), static_cast<float>(y)};
}

float Input::GetMouseX() {
    return GetMousePosition().x;
}

float Input::GetMouseY() {
    return GetMousePosition().y;
}

} // namespace Puluo
