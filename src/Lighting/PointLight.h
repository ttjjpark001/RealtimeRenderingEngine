#pragma once

#include <DirectXMath.h>
#include "Core/Types.h"

namespace RRE
{

class PointLight
{
public:
    PointLight() = default;
    ~PointLight() = default;

    void SetPosition(const DirectX::XMFLOAT3& pos) { m_position = pos; }
    const DirectX::XMFLOAT3& GetPosition() const { return m_position; }

    void SetColor(const DirectX::XMFLOAT3& color) { m_color = color; }
    const DirectX::XMFLOAT3& GetColor() const { return m_color; }

    float GetConstantAttenuation() const { return m_Kc; }
    float GetLinearAttenuation() const { return m_Kl; }
    float GetQuadraticAttenuation() const { return m_Kq; }

    const char* GetColorName() const
    {
        if (m_color.x == 1.0f && m_color.y == 1.0f && m_color.z == 1.0f) return "White";
        if (m_color.x == 1.0f && m_color.y == 0.0f && m_color.z == 0.0f) return "Red";
        if (m_color.x == 0.0f && m_color.y == 1.0f && m_color.z == 0.0f) return "Green";
        if (m_color.x == 0.0f && m_color.y == 0.0f && m_color.z == 1.0f) return "Blue";
        if (m_color.x == 1.0f && m_color.y == 1.0f && m_color.z == 0.0f) return "Yellow";
        if (m_color.x == 0.0f && m_color.y == 1.0f && m_color.z == 1.0f) return "Cyan";
        if (m_color.x == 1.0f && m_color.y == 0.0f && m_color.z == 1.0f) return "Magenta";
        return "Custom";
    }

    void Reset()
    {
        m_position = { 2.0f, 3.0f, -2.0f };
        m_color = { 1.0f, 1.0f, 1.0f };
    }

private:
    DirectX::XMFLOAT3 m_position = { 2.0f, 3.0f, -2.0f };
    DirectX::XMFLOAT3 m_color = { 1.0f, 1.0f, 1.0f };
    float m_Kc = 1.0f;
    float m_Kl = 0.09f;
    float m_Kq = 0.032f;
};

} // namespace RRE
