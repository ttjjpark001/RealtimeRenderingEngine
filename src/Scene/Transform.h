#pragma once

#include <DirectXMath.h>

namespace RRE
{

class Transform
{
public:
    Transform() = default;
    ~Transform() = default;

    DirectX::XMMATRIX GetLocalMatrix() const;

    void SetPosition(const DirectX::XMFLOAT3& pos) { m_position = pos; m_useDirectMatrix = false; }
    void SetRotation(const DirectX::XMFLOAT3& rot) { m_rotation = rot; m_useDirectMatrix = false; }
    void SetScale(const DirectX::XMFLOAT3& s) { m_scale = s; m_useDirectMatrix = false; }

    // Set the local matrix directly, bypassing TRS decomposition/recomposition.
    // Used when importing transforms from external formats (e.g., Assimp)
    // to avoid lossy Euler angle round-trips.
    void SetLocalMatrix(const DirectX::XMMATRIX& matrix);

    const DirectX::XMFLOAT3& GetPosition() const { return m_position; }
    const DirectX::XMFLOAT3& GetRotation() const { return m_rotation; }
    const DirectX::XMFLOAT3& GetScale() const { return m_scale; }

private:
    DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_rotation = { 0.0f, 0.0f, 0.0f }; // Euler angles in radians
    DirectX::XMFLOAT3 m_scale    = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4X4 m_directMatrix;
    bool m_useDirectMatrix = false;
};

} // namespace RRE
