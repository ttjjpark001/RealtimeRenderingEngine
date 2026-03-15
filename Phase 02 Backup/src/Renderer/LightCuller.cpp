#include "Renderer/LightCuller.h"
#include "Lighting/Light.h"
#include "Lighting/LightManager.h"
#include <cmath>

using namespace DirectX;

namespace RRE
{

float LightCuller::ComputeEffectiveRadius(const Light& light, float threshold)
{
    // Solve Kc + Kl*d + Kq*d^2 = intensity/threshold for d > 0.
    float target = light.intensity / threshold;
    float a = light.Kq;
    float b = light.Kl;
    float c = light.Kc - target;

    if (a < 1e-6f)
    {
        // Linear or constant attenuation
        if (b < 1e-6f) return 1e6f;  // constant — infinite range
        return std::max(0.0f, -c / b);
    }

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f)
        return 1e6f;

    return (-b + std::sqrtf(discriminant)) / (2.0f * a);
}

std::vector<uint32_t> LightCuller::CullLights(
    const BoundingFrustum& frustum,
    const XMFLOAT3& cameraPos,
    const LightManager& lightManager,
    float contributionThreshold)
{
    std::vector<uint32_t> active;
    uint32_t total   = static_cast<uint32_t>(lightManager.GetActiveLightCount());
    uint32_t culled  = 0;

    XMVECTOR camV = XMLoadFloat3(&cameraPos);

    for (uint32_t i = 0; i < total; i++)
    {
        const Light& light = lightManager.GetLight(i);

        // Directional lights illuminate everything — never cull.
        if (light.type == LightType::Directional)
        {
            active.push_back(i);
            continue;
        }

        // Compute effective radius (distance where contribution drops to threshold).
        float radius = ComputeEffectiveRadius(light, contributionThreshold);

        // Frustum intersection test: if the light's influence sphere doesn't touch
        // the frustum, the light cannot contribute to visible pixels.
        BoundingSphere lightSphere(light.position, radius);
        if (!frustum.Intersects(lightSphere))
        {
            culled++;
            continue;
        }

        // Contribution estimate at the camera position.
        // This is a coarse proxy for "is this light noticeable on screen?"
        XMVECTOR lightV = XMLoadFloat3(&light.position);
        float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(lightV, camV)));
        float denom = light.Kc + light.Kl * dist + light.Kq * dist * dist;
        float contrib = light.intensity / std::max(denom, 1e-3f);

        if (contrib < contributionThreshold)
        {
            culled++;
            continue;
        }

        active.push_back(i);
    }

    m_lastCulledCount = culled;
    m_lastActiveCount = static_cast<uint32_t>(active.size());
    return active;
}

} // namespace RRE
