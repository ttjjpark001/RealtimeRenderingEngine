#pragma once

#include "Lighting/Light.h"
#include "Core/Types.h"
#include <vector>

namespace RRE
{

struct LightConstants;
struct ShadowConstants;

class LightManager
{
public:
    LightManager() = default;
    ~LightManager() = default;

    size_t AddLight(const Light& light);
    void RemoveLight(size_t index);

    const Light& GetLight(size_t index) const { return m_lights[index]; }
    Light& GetLightMutable(size_t index) { return m_lights[index]; }

    size_t GetActiveLightCount() const { return m_lights.size(); }

    // Build GPU-ready LightConstants (capped at MAX_PBR_LIGHTS)
    // Also assigns shadowMapIndex for castShadow lights
    LightConstants BuildLightConstants() const;

    // Build LightConstants using only the lights at the given indices
    // (e.g. as returned by LightCuller::CullLights). Also updates shadowCasterCount.
    LightConstants BuildFilteredLightConstants(
        const std::vector<uint32_t>& activeIndices) const;

    // Texture2D shadow casters (Directional/Spot)
    uint32 GetShadowCasterCount() const      { return m_shadowCasterCount; }
    // TextureCube shadow casters (Point light)
    uint32 GetPointShadowCasterCount() const { return m_pointShadowCasterCount; }

    void Clear()
    {
        m_lights.clear();
        m_shadowCasterCount = 0;
        m_pointShadowCasterCount = 0;
    }

private:
    std::vector<Light> m_lights;
    mutable uint32 m_shadowCasterCount      = 0;
    mutable uint32 m_pointShadowCasterCount = 0;
};

} // namespace RRE
