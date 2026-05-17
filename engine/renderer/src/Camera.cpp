#include "puluo/renderer/Camera.h"
#include "puluo/core/Input.h"
#include "puluo/core/Application.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Puluo {

// ---- Camera ----

Camera Camera::Perspective(float fovDegrees, float aspect, float nearClip, float farClip) {
    Camera cam;
    cam.m_Projection = glm::perspective(glm::radians(fovDegrees), aspect, nearClip, farClip);
    return cam;
}

Camera Camera::Orthographic(float left, float right, float bottom, float top,
                             float nearClip, float farClip) {
    Camera cam;
    cam.m_Projection = glm::ortho(left, right, bottom, top, nearClip, farClip);
    return cam;
}

// ---- CameraController ----

CameraController::CameraController(float fovDegrees, float aspectRatio)
    : m_Fov(fovDegrees), m_AspectRatio(aspectRatio)
{
    m_Camera = Camera::Perspective(m_Fov, m_AspectRatio, m_NearClip, m_FarClip);
    UpdateVectors();
    UpdateViewMatrix();
}

void CameraController::OnUpdate(float deltaTime) {
    // Skip input when disabled (e.g., ImGui is capturing)
    if (!m_InputEnabled) {
        if (m_MouseCaptured) {
            Application::Get().GetWindow().SetCursorMode(true);
            m_MouseCaptured = false;
        }
        UpdateViewMatrix();
        return;
    }

    // Right-click to enable FPS control
    if (Input::IsMouseButtonPressed(MouseButton::Right)) {
        if (!m_MouseCaptured) {
            Application::Get().GetWindow().SetCursorMode(false);
            m_MouseCaptured = true;
            m_FirstMouse = true;
        }

        // Keyboard movement
        float velocity = m_MoveSpeed * deltaTime;
        if (Input::IsKeyPressed(Key::W)) m_Position += m_Front * velocity;
        if (Input::IsKeyPressed(Key::S)) m_Position -= m_Front * velocity;
        if (Input::IsKeyPressed(Key::A)) m_Position -= m_Right * velocity;
        if (Input::IsKeyPressed(Key::D)) m_Position += m_Right * velocity;
        if (Input::IsKeyPressed(Key::E)) m_Position += m_Up * velocity;
        if (Input::IsKeyPressed(Key::Q)) m_Position -= m_Up * velocity;

        // Mouse look
        Vec2 mousePos = Input::GetMousePosition();

        if (m_FirstMouse) {
            m_LastMouseX = mousePos.x;
            m_LastMouseY = mousePos.y;
            m_FirstMouse = false;
        }

        float xOffset = mousePos.x - m_LastMouseX;
        float yOffset = m_LastMouseY - mousePos.y;
        m_LastMouseX = mousePos.x;
        m_LastMouseY = mousePos.y;

        xOffset *= m_MouseSensitivity;
        yOffset *= m_MouseSensitivity;

        m_Yaw += xOffset;
        m_Pitch += yOffset;

        if (m_Pitch > 89.0f) m_Pitch = 89.0f;
        if (m_Pitch < -89.0f) m_Pitch = -89.0f;

        UpdateVectors();
    } else {
        if (m_MouseCaptured) {
            Application::Get().GetWindow().SetCursorMode(true);
            m_MouseCaptured = false;
        }
    }

    UpdateViewMatrix();
}

void CameraController::SetAspectRatio(float ratio) {
    m_AspectRatio = ratio;
    UpdateProjection();
}

void CameraController::UpdateVectors() {
    Vec3 front;
    front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    front.y = sin(glm::radians(m_Pitch));
    front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    m_Front = glm::normalize(front);
    m_Right = glm::normalize(glm::cross(m_Front, Vec3(0.0f, 1.0f, 0.0f)));
    m_Up = glm::normalize(glm::cross(m_Right, m_Front));
}

void CameraController::UpdateViewMatrix() {
    m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Front, m_Up);
}

void CameraController::UpdateProjection() {
    m_Camera = Camera::Perspective(m_Fov, m_AspectRatio, m_NearClip, m_FarClip);
}

} // namespace Puluo
