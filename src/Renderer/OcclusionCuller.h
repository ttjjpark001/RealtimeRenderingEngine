#pragma once

#include "Core/Types.h"
#include <unordered_map>

namespace RRE
{

class SceneNode;

// GPU Hi-Z Occlusion Culler (Phase 32).
// Stores the per-node occlusion results produced by the previous frame's
// GPU compute test (1-frame latency). IsOccluded() returns false for any
// node not yet tested — conservative, never causes incorrect culling.
class OcclusionCuller
{
public:
    OcclusionCuller() = default;

    // Update the result table from GPU readback data.
    // nodes[i] corresponds to results[i] (0=visible, 1=occluded).
    void UpdateResults(const SceneNode* const* nodes, const uint32* results, uint32 count);

    // Returns true only if the GPU test confirmed this node was fully occluded
    // in the previous frame. Defaults to false (visible) for untested nodes.
    bool IsOccluded(const SceneNode* node) const;

    // Clear all results (call on scene change)
    void ClearResults() { m_results.clear(); }

private:
    std::unordered_map<const SceneNode*, bool> m_results;
};

} // namespace RRE
