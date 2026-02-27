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

    // Get count of shadow-casting lights (after BuildLightConstants)
    uint32 GetShadowCasterCount() const { return m_shadowCasterCount; }

    void Clear() { m_lights.clear(); m_shadowCasterCount = 0; }

private:
    std::vector<Light> m_lights;
    mutable uint32 m_shadowCasterCount = 0;
};

} // namespace RRE
