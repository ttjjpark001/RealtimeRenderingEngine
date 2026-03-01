#include "Scene/Camera.h"
#include <cmath>

using namespace DirectX;

namespace RRE
{

XMMATRIX Camera::GetViewMatrix() const
{
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR target = XMLoadFloat3(&m_lookAt);
    XMVECTOR up = XMLoadFloat3(&m_up);
    return XMMatrixLookAtLH(pos, target, up);
}

XMMATRIX Camera::GetProjectionMatrix(float aspectRatio) const
{
    if (m_projectionMode == ProjectionMode::Orthographic)
    {
        return XMMatrixOrthographicLH(
            m_orthoSize * aspectRatio * 2.0f,
            m_orthoSize * 2.0f,
            m_nearPlane, m_farPlane);
    }

    return XMMatrixPerspectiveFovLH(m_fov, aspectRatio, m_nearPlane, m_farPlane);
}

XMFLOAT3 Camera::GetDirection() const
{
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR target = XMLoadFloat3(&m_lookAt);
    XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMFLOAT3 result;
    XMStoreFloat3(&result, dir);
    return result;
}

float Camera::GetFovDegrees() const
{
    return XMConvertToDegrees(m_fov);
}

const char* Camera::GetProjectionModeName() const
{
    return m_projectionMode == ProjectionMode::Perspective ? "Perspective" : "Orthographic";
}

void Camera::MoveForward(float distance)
{
    float scaledDist = distance * m_moveSpeedScale;
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR target = XMLoadFloat3(&m_lookAt);
    XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMVECTOR offset = XMVectorScale(dir, scaledDist);

    pos = XMVectorAdd(pos, offset);
    target = XMVectorAdd(target, offset);

    XMStoreFloat3(&m_position, pos);
    XMStoreFloat3(&m_lookAt, target);
}

void Camera::MoveRight(float distance)
{
    float scaledDist = distance * m_moveSpeedScale;
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR target = XMLoadFloat3(&m_lookAt);
    XMVECTOR up = XMLoadFloat3(&m_up);
    XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
    XMVECTOR offset = XMVectorScale(right, scaledDist);

    pos = XMVectorAdd(pos, offset);
    target = XMVectorAdd(target, offset);

    XMStoreFloat3(&m_position, pos);
    XMStoreFloat3(&m_lookAt, target);
}

void Camera::MoveUp(float distance)
{
    float scaledDist = distance * m_moveSpeedScale;
    m_position.y += scaledDist;
    m_lookAt.y += scaledDist;
}

void Camera::Rotate(float yawDelta, float pitchDelta)
{
    if (!m_yawPitchInitialized)
    {
        RecalcYawPitchFromLookAt();
        m_yawPitchInitialized = true;
    }

    m_yaw += yawDelta;
    m_pitch += pitchDelta;

    // Clamp pitch to ±89 degrees
    constexpr float maxPitch = XMConvertToRadians(89.0f);
    if (m_pitch > maxPitch) m_pitch = maxPitch;
    if (m_pitch < -maxPitch) m_pitch = -maxPitch;

    RecalcLookAtFromYawPitch();
}

void Camera::FitToScene(const XMFLOAT3& sceneCenter, float sceneDiagonal)
{
    m_lookAt = sceneCenter;

    // Elevated front-side view: camera above and in front at ~45 degrees.
    // Works for both exterior objects and open-top interior scenes (e.g. Sponza atrium).
    float half = sceneDiagonal * 0.5f;
    if (half < 0.5f) half = 0.5f;

    m_position = {
        sceneCenter.x,
        sceneCenter.y + half,   // elevated above center
        sceneCenter.z - half    // in front of center (-Z)
    };

    m_up = { 0.0f, 1.0f, 0.0f };

    // Far plane: camera-to-center distance + full diagonal for margin
    float distToCenter = half * 1.4142f;  // sqrt(2) * half
    m_farPlane = (distToCenter + sceneDiagonal) * 3.0f;
    if (m_farPlane < 100.0f) m_farPlane = 100.0f;

    RecalcYawPitchFromLookAt();
    m_yawPitchInitialized = true;
}

void Camera::RecalcYawPitchFromLookAt()
{
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR target = XMLoadFloat3(&m_lookAt);
    XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(target, pos));

    XMFLOAT3 d;
    XMStoreFloat3(&d, dir);

    m_yaw = std::atan2f(d.x, d.z);
    m_pitch = std::asinf(d.y);
}

void Camera::RecalcLookAtFromYawPitch()
{
    float cosPitch = std::cosf(m_pitch);
    XMFLOAT3 dir = {
        cosPitch * std::sinf(m_yaw),
        std::sinf(m_pitch),
        cosPitch * std::cosf(m_yaw)
    };

    m_lookAt = {
        m_position.x + dir.x,
        m_position.y + dir.y,
        m_position.z + dir.z
    };
}

void Camera::AdjustFov(float deltaDegrees)
{
    float degrees = XMConvertToDegrees(m_fov) + deltaDegrees;
    if (degrees < 10.0f) degrees = 10.0f;
    if (degrees > 120.0f) degrees = 120.0f;
    m_fov = XMConvertToRadians(degrees);
}

void Camera::Reset()
{
    m_position = { 0.0f, 0.0f, -5.0f };
    m_lookAt = { 0.0f, 0.0f, 0.0f };
    m_up = { 0.0f, 1.0f, 0.0f };
    m_fov = XM_PIDIV4;
    m_nearPlane = 0.1f;
    m_farPlane = 100.0f;
    m_orthoSize = 5.0f;
    m_projectionMode = ProjectionMode::Perspective;
    m_moveSpeedScale = 1.0f;
    RecalcYawPitchFromLookAt();
    m_yawPitchInitialized = true;
}

} // namespace RRE
