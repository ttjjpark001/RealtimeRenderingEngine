#include "Renderer/FrustumCuller.h"

using namespace DirectX;

namespace RRE
{

void FrustumCuller::Build(const XMMATRIX& view, const XMMATRIX& projection)
{
    // Create frustum in view space from the projection matrix alone.
    BoundingFrustum frustumVS;
    BoundingFrustum::CreateFromMatrix(frustumVS, projection);

    // Transform to world space using the inverse view matrix.
    XMVECTOR det;
    XMMATRIX invView = XMMatrixInverse(&det, view);
    frustumVS.Transform(m_frustum, invView);

    m_isBuilt = true;
}

bool FrustumCuller::IsVisible(const BoundingBox& worldAABB) const
{
    if (!m_isBuilt)
        return true;  // conservative: never cull before first Build()

    return m_frustum.Intersects(worldAABB);
}

} // namespace RRE
