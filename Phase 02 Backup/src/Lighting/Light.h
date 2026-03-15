#pragma once

#include <DirectXMath.h>

namespace RRE
{

enum class LightType : uint32_t
{
    Directional = 0,
    Point = 1,
    Spot = 2
};

struct Light
{
    LightType type = LightType::Point;

    DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };    // Point, Spot
    DirectX::XMFLOAT3 direction = { 0.0f, -1.0f, 0.0f };   // Directional, Spot

    float Kc = 1.0f;    // Constant attenuation
    float Kl = 0.09f;   // Linear attenuation
    float Kq = 0.032f;  // Quadratic attenuation

    float innerConeAngle = 0.9063f;  // Spot inner cone (cos(25 deg))
    float outerConeAngle = 0.8192f;  // Spot outer cone (cos(35 deg))

    bool castShadow = false;
};

} // namespace RRE
