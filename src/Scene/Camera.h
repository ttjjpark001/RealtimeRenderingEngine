#pragma once

#include <DirectXMath.h>

namespace RRE
{

enum class ProjectionMode
{
    Perspective,
    Orthographic
};

class Camera
{
public:
    Camera() = default;
    ~Camera() = default;

    // Matrix generation
    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix(float aspectRatio) const;

    // Direction
    DirectX::XMFLOAT3 GetDirection() const;

    // Accessors
    const DirectX::XMFLOAT3& GetPosition() const { return m_position; }
    const DirectX::XMFLOAT3& GetLookAt() const { return m_lookAt; }
    float GetFov() const { return m_fov; }
    float GetFovDegrees() const;
    ProjectionMode GetProjectionMode() const { return m_projectionMode; }
    const char* GetProjectionModeName() const;

    // Mutators
    void SetPosition(const DirectX::XMFLOAT3& pos) { m_position = pos; }
    void SetLookAt(const DirectX::XMFLOAT3& target) { m_lookAt = target; }
    void SetFov(float radians) { m_fov = radians; }
    void SetNearPlane(float n) { m_nearPlane = n; }
    void SetFarPlane(float f) { m_farPlane = f; }
    float GetNearPlane() const { return m_nearPlane; }
    float GetFarPlane() const { return m_farPlane; }
    void SetProjectionMode(ProjectionMode mode) { m_projectionMode = mode; }

    // Movement
    void MoveForward(float distance);
    void MoveRight(float distance);
    void MoveUp(float distance);

    // Yaw/Pitch rotation (FPS-style, radians)
    void Rotate(float yawDelta, float pitchDelta);

    // Fit camera to view entire scene
    void FitToScene(const DirectX::XMFLOAT3& sceneCenter, float sceneDiagonal);

    // Move speed scale (proportional to scene size)
    void SetMoveSpeedScale(float scale) { m_moveSpeedScale = scale; }
    float GetMoveSpeedScale() const { return m_moveSpeedScale; }

    // FOV adjustment (degrees, clamped 10~120)
    void AdjustFov(float deltaDegrees);

    // Reset to defaults
    void Reset();

private:
    DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, -5.0f };
    DirectX::XMFLOAT3 m_lookAt = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_up = { 0.0f, 1.0f, 0.0f };
    float m_fov = DirectX::XM_PIDIV4;  // 45 degrees
    float m_nearPlane = 0.1f;
    float m_farPlane = 100.0f;
    float m_orthoSize = 5.0f;
    ProjectionMode m_projectionMode = ProjectionMode::Perspective;

    float m_yaw = 0.0f;        // horizontal look angle (radians)
    float m_pitch = 0.0f;      // vertical look angle (radians, clamped ±89°)
    float m_moveSpeedScale = 1.0f;
    bool m_yawPitchInitialized = false;

    void RecalcYawPitchFromLookAt();
    void RecalcLookAtFromYawPitch();
};

} // namespace RRE
