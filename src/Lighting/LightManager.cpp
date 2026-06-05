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

    uint32 shadowIdx = 0;   // Texture2D (Directional/Spot)
    uint32 cubeIdx   = 0;   // TextureCube (Point)
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
        dst.shadowType = 0;
        dst.shadowMapIndex = -1;

        if (src.castShadow)
        {
            if (src.type == LightType::Point && cubeIdx < 4u)
            {
                dst.shadowType     = 1;  // TextureCube
                dst.shadowMapIndex = static_cast<int32>(cubeIdx);
                cubeIdx++;
            }
            else if (src.type != LightType::Point && shadowIdx < MAX_SHADOW_MAPS)
            {
                dst.shadowType     = 0;  // Texture2D
                dst.shadowMapIndex = static_cast<int32>(shadowIdx);
                shadowIdx++;
            }
        }
    }

    m_shadowCasterCount      = shadowIdx;
    m_pointShadowCasterCount = cubeIdx;
    return constants;
}

LightConstants LightManager::BuildFilteredLightConstants(
    const std::vector<uint32_t>& activeIndices) const
{
    LightConstants constants = {};

    uint32 count = static_cast<uint32>(
        (std::min)(activeIndices.size(), static_cast<size_t>(MAX_PBR_LIGHTS)));
    constants.numActiveLights = count;

    uint32 shadowIdx = 0;
    uint32 cubeIdx   = 0;
    for (uint32 i = 0; i < count; i++)
    {
        uint32_t srcIdx = activeIndices[i];
        if (srcIdx >= static_cast<uint32_t>(m_lights.size()))
            continue;

        const Light& src = m_lights[srcIdx];
        GPULightData& dst = constants.lights[i];

        dst.position  = src.position;
        dst.intensity = src.intensity;
        dst.color     = src.color;
        dst.Kc        = src.Kc;
        dst.Kl        = src.Kl;
        dst.Kq        = src.Kq;
        dst.type      = static_cast<uint32>(src.type);
        dst.direction = src.direction;
        dst.innerConeAngle = src.innerConeAngle;
        dst.outerConeAngle = src.outerConeAngle;
        dst.shadowType     = 0;
        dst.shadowMapIndex = -1;

        if (src.castShadow)
        {
            if (src.type == LightType::Point && cubeIdx < 4u)
            {
                dst.shadowType     = 1;
                dst.shadowMapIndex = static_cast<int32>(cubeIdx);
                cubeIdx++;
            }
            else if (src.type != LightType::Point && shadowIdx < MAX_SHADOW_MAPS)
            {
                dst.shadowType     = 0;
                dst.shadowMapIndex = static_cast<int32>(shadowIdx);
                shadowIdx++;
            }
        }
    }

    m_shadowCasterCount      = shadowIdx;
    m_pointShadowCasterCount = cubeIdx;
    return constants;
}

} // namespace RRE
