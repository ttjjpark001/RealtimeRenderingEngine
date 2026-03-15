#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>

namespace RRE
{

// P0 stub: CPU readback-based occlusion culler.
// Always returns false (conservative — never culls) until a full Hi-Z implementation
// is provided in a future phase. Preserves the interface so callers compile unchanged.
class OcclusionCuller
{
public:
    OcclusionCuller() = default;

    // Returns true if the AABB is fully occluded by previously rendered geometry.
    // P0: always returns false (no false negatives — objects are never incorrectly hidden).
    bool IsOccluded(const DirectX::BoundingBox& worldAABB,
                    const DirectX::XMMATRIX& viewProj) const;
};

} // namespace RRE
