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

    void SetPosition(const DirectX::XMFLOAT3& pos) { m_position = pos; }
    void SetRotation(const DirectX::XMFLOAT3& rot) { m_rotation = rot; }
    void SetScale(const DirectX::XMFLOAT3& s) { m_scale = s; }

    const DirectX::XMFLOAT3& GetPosition() const { return m_position; }
    const DirectX::XMFLOAT3& GetRotation() const { return m_rotation; }
    const DirectX::XMFLOAT3& GetScale() const { return m_scale; }

private:
    DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_rotation = { 0.0f, 0.0f, 0.0f }; // Euler angles in radians
    DirectX::XMFLOAT3 m_scale    = { 1.0f, 1.0f, 1.0f };
};

} // namespace RRE
