#include "Lighting/LightManager.h"
#include "RHI/D3D12/D3D12Context.h"
#include <algorithm>

namespace RRE
{

size_t LightManager::AddLight(const Light& light)
{
    m_lights.push_back(light);
    return m_lights.size() - 1;
}

void LightManager::RemoveLight(size_t index)
{
    if (index < m_lights.size())
        m_lights.erase(m_lights.begin() + static_cast<ptrdiff_t>(index));
}

LightConstants LightManager::BuildLightConstants() const
{
    LightConstants constants = {};

    uint32 count = static_cast<uint32>(
        (std::min)(m_lights.size(), static_cast<size_t>(MAX_PBR_LIGHTS)));
    constants.numActiveLights = count;

    uint32 shadowIdx = 0;
    for (uint32 i = 0; i < count; i++)
    {
        const Light& src = m_lights[i];
        GPULightData& dst = constants.lights[i];

        dst.position = src.position;
        dst.intensity = src.intensity;
        dst.color = src.color;
        dst.Kc = src.Kc;
        dst.Kl = src.Kl;
        dst.Kq = src.Kq;
        dst.type = static_cast<uint32>(src.type);
        dst.direction = src.direction;
        dst.innerConeAngle = src.innerConeAngle;
        dst.outerConeAngle = src.outerConeAngle;

        // Assign shadow map index for castShadow lights (max 8)
        if (src.castShadow && shadowIdx < MAX_SHADOW_MAPS)
        {
            dst.shadowMapIndex = static_cast<int32>(shadowIdx);
            shadowIdx++;
        }
        else
        {
            dst.shadowMapIndex = -1;
        }
    }

    m_shadowCasterCount = shadowIdx;
    return constants;
}

} // namespace RRE
