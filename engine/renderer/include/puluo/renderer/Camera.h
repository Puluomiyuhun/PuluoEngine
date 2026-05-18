#pragma once

#include "puluo/core/Math.h"

namespace Puluo {

class Camera {
public:
    Camera() = default;

    static Camera Perspective(float fovDegrees, float aspect, float nearClip, float farClip);
    static Camera Orthographic(float left, float right, float bottom, float top,
                                float nearClip = -1.0f, float farClip = 1.0f);

    const Mat4& GetProjection() const { return m_Projection; }
    void SetProjection(const Mat4& projection) { m_Projection = projection; }

private:
    Mat4 m_Projection{1.0f};
};

// FPS-style camera controller
class CameraController {
public:
    CameraController() = default;
    CameraController(float fovDegrees, float aspectRatio);

    void OnUpdate(float deltaTime);

    const Mat4& GetViewMatrix() const { return m_ViewMatrix; }
    const Mat4& GetProjectionMatrix() const { return m_Camera.GetProjection(); }
    Mat4 GetViewProjection() const { return m_Camera.GetProjection() * m_ViewMatrix; }

    // TAA jitter support
    void SetJitter(float jx, float jy);
    void ClearJitter();
    const Mat4& GetProjectionMatrixJittered() const { return m_JitteredProjection; }
    const Mat4& GetProjectionMatrixUnjittered() const { return m_Camera.GetProjection(); }
    Vec2 GetJitter() const { return m_Jitter; }
    Vec2 GetPrevJitter() const { return m_PrevJitter; }

    void SetAspectRatio(float ratio);
    const Vec3& GetPosition() const { return m_Position; }

    void SetInputEnabled(bool enabled) { m_InputEnabled = enabled; }

    float GetMoveSpeed() const { return m_MoveSpeed; }
    void SetMoveSpeed(float speed) { m_MoveSpeed = speed; }
    float GetMouseSensitivity() const { return m_MouseSensitivity; }
    void SetMouseSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }

    // Serialization helpers
    void SetPosition(const Vec3& pos) { m_Position = pos; UpdateVectors(); UpdateViewMatrix(); }
    float GetYaw() const { return m_Yaw; }
    void SetYaw(float yaw) { m_Yaw = yaw; UpdateVectors(); UpdateViewMatrix(); }
    float GetPitch() const { return m_Pitch; }
    void SetPitch(float pitch) { m_Pitch = pitch; UpdateVectors(); UpdateViewMatrix(); }
    float GetFov() const { return m_Fov; }
    float GetNearClip() const { return m_NearClip; }
    float GetFarClip() const { return m_FarClip; }
    float GetAspectRatio() const { return m_AspectRatio; }
    void SetFov(float fov) { m_Fov = fov; UpdateProjection(); }
    void RestoreState(const Vec3& pos, float yaw, float pitch, float fov, float speed, float sensitivity) {
        m_Position = pos; m_Yaw = yaw; m_Pitch = pitch; m_Fov = fov;
        m_MoveSpeed = speed; m_MouseSensitivity = sensitivity;
        UpdateVectors(); UpdateViewMatrix(); UpdateProjection();
    }

private:
    Camera m_Camera;
    Mat4 m_ViewMatrix{1.0f};

    Vec3 m_Position{0.0f, 1.0f, 5.0f};
    Vec3 m_Front{0.0f, 0.0f, -1.0f};
    Vec3 m_Up{0.0f, 1.0f, 0.0f};
    Vec3 m_Right{1.0f, 0.0f, 0.0f};

    float m_Yaw = -90.0f;
    float m_Pitch = 0.0f;
    float m_MoveSpeed = 5.0f;
    float m_MouseSensitivity = 0.1f;
    float m_Fov = 45.0f;
    float m_AspectRatio = 16.0f / 9.0f;
    float m_NearClip = 0.1f;
    float m_FarClip = 10000.0f;

    float m_LastMouseX = 0.0f;
    float m_LastMouseY = 0.0f;
    bool m_FirstMouse = true;
    bool m_MouseCaptured = false;
    bool m_InputEnabled = true;

    // TAA jitter state
    Mat4 m_JitteredProjection{1.0f};
    Vec2 m_Jitter{0.0f};
    Vec2 m_PrevJitter{0.0f};

    void UpdateVectors();
    void UpdateViewMatrix();
    void UpdateProjection();
};

} // namespace Puluo
