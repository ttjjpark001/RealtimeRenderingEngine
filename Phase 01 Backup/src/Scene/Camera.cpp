#include "Scene/Camera.h"

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
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR target = XMLoadFloat3(&m_lookAt);
    XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMVECTOR offset = XMVectorScale(dir, distance);

    pos = XMVectorAdd(pos, offset);
    target = XMVectorAdd(target, offset);

    XMStoreFloat3(&m_position, pos);
    XMStoreFloat3(&m_lookAt, target);
}

void Camera::MoveRight(float distance)
{
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR target = XMLoadFloat3(&m_lookAt);
    XMVECTOR up = XMLoadFloat3(&m_up);
    XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
    XMVECTOR offset = XMVectorScale(right, distance);

    pos = XMVectorAdd(pos, offset);
    target = XMVectorAdd(target, offset);

    XMStoreFloat3(&m_position, pos);
    XMStoreFloat3(&m_lookAt, target);
}

void Camera::MoveUp(float distance)
{
    m_position.y += distance;
    m_lookAt.y += distance;
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
}

} // namespace RRE
