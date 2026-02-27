#pragma once

#include "Lighting/Light.h"
#include "Core/Types.h"
#include <vector>

namespace RRE
{

struct LightConstants;

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
    LightConstants BuildLightConstants() const;

    void Clear() { m_lights.clear(); }

private:
    std::vector<Light> m_lights;
};

} // namespace RRE
