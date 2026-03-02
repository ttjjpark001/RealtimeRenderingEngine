#include "Renderer/OcclusionCuller.h"

namespace RRE
{

bool OcclusionCuller::IsOccluded(const DirectX::BoundingBox& /*worldAABB*/,
                                  const DirectX::XMMATRIX& /*viewProj*/) const
{
    // P0 stub: never report an object as occluded.
    // A future phase will implement Hi-Z buffer readback to properly detect
    // objects fully occluded by closer geometry.
    return false;
}

} // namespace RRE
