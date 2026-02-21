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
    void SetProjectionMode(ProjectionMode mode) { m_projectionMode = mode; }

    // Movement
    void MoveForward(float distance);
    void MoveRight(float distance);
    void MoveUp(float distance);

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
};

} // namespace RRE
