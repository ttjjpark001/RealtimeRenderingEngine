#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>

namespace RRE
{

// Builds a world-space BoundingFrustum from camera view/projection matrices
// and tests BoundingBox visibility against it.
class FrustumCuller
{
public:
    FrustumCuller() = default;

    // Build world-space frustum from view and projection matrices.
    // Must be called each frame before IsVisible().
    void Build(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);

    // Returns true if the world-space AABB is (at least partially) inside the frustum.
    // Returns true if Build() has not been called yet (conservative: never culls).
    bool IsVisible(const DirectX::BoundingBox& worldAABB) const;

    const DirectX::BoundingFrustum& GetFrustum() const { return m_frustum; }

    bool IsBuilt() const { return m_isBuilt; }

private:
    DirectX::BoundingFrustum m_frustum;
    bool m_isBuilt = false;
};

} // namespace RRE
