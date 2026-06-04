#include "Renderer/OcclusionCuller.h"
#include "Scene/SceneNode.h"

namespace RRE
{

void OcclusionCuller::UpdateResults(const SceneNode* const* nodes,
                                     const uint32* results,
                                     uint32 count)
{
    m_results.clear();
    for (uint32 i = 0; i < count; i++)
        m_results[nodes[i]] = (results[i] != 0u);
}

bool OcclusionCuller::IsOccluded(const SceneNode* node) const
{
    auto it = m_results.find(node);
    if (it == m_results.end())
        return false;   // not yet tested → treat as visible (conservative)
    return it->second;
}

} // namespace RRE
