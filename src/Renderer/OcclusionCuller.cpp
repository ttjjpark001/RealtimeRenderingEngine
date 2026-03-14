#include "Renderer/OcclusionCuller.h"

namespace RRE
{

bool OcclusionCuller::IsOccluded([[maybe_unused]] const DirectX::BoundingBox& worldAABB,
                                  [[maybe_unused]] const DirectX::XMMATRIX& viewProj) const
{
    // P0 stub: never report an object as occluded.
    // A future phase will implement Hi-Z buffer readback to properly detect
    // objects fully occluded by closer geometry.
    return false;
}

} // namespace RRE
