#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <vector>

namespace RRE
{

class LightManager;
struct Light;

// Culls Point/Spot lights whose contribution to the visible scene is negligible.
// Directional lights are never culled (infinite range).
class LightCuller
{
public:
    LightCuller() = default;

    // Returns indices (into LightManager) of lights that should be submitted to the GPU.
    // Culling criteria:
    //   1. Frustum: light's effective-range BoundingSphere does not intersect the frustum
    //   2. Contribution: estimated irradiance at the camera position is below the threshold
    std::vector<uint32_t> CullLights(
        const DirectX::BoundingFrustum& frustum,
        const DirectX::XMFLOAT3& cameraPos,
        const LightManager& lightManager,
        float contributionThreshold = 0.01f);

    uint32_t GetLastCulledCount()  const { return m_lastCulledCount; }
    uint32_t GetLastActiveCount()  const { return m_lastActiveCount; }

private:
    // Distance at which the light's attenuation drops the contribution below `threshold`.
    static float ComputeEffectiveRadius(const Light& light, float threshold);

    uint32_t m_lastCulledCount = 0;
    uint32_t m_lastActiveCount = 0;
};

} // namespace RRE
