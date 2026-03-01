#include "Scene/Transform.h"
#include "Math/MathUtil.h"

namespace RRE
{

DirectX::XMMATRIX Transform::GetLocalMatrix() const
{
    if (m_useDirectMatrix)
        return DirectX::XMLoadFloat4x4(&m_directMatrix);

    return Math::CreateTRSMatrix(m_position, m_rotation, m_scale);
}

void Transform::SetLocalMatrix(const DirectX::XMMATRIX& matrix)
{
    DirectX::XMStoreFloat4x4(&m_directMatrix, matrix);
    m_useDirectMatrix = true;

    // Also decompose for GetPosition/GetScale accessors
    DirectX::XMVECTOR scale, rotQuat, translation;
    if (DirectX::XMMatrixDecompose(&scale, &rotQuat, &translation, matrix))
    {
        DirectX::XMStoreFloat3(&m_position, translation);
        DirectX::XMStoreFloat3(&m_scale, scale);
        // Store zero rotation — the direct matrix is authoritative
        m_rotation = { 0.0f, 0.0f, 0.0f };
    }
}

} // namespace RRE
