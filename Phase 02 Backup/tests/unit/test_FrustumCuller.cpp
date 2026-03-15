#include <gtest/gtest.h>
#include "Renderer/FrustumCuller.h"
#include "Scene/Camera.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>

using namespace DirectX;
using namespace RRE;

// Helper: build a frustum from a standard test camera
//   position = (0, 0, -5), lookAt = (0, 0, 0), up = (0, 1, 0)
//   FOV = 45 deg, aspect = 1:1, near = 0.1, far = 100
static FrustumCuller BuildTestFrustum()
{
    Camera cam;
    cam.Reset();  // pos(0,0,-5), lookAt(0,0,0)

    XMMATRIX view = cam.GetViewMatrix();
    XMMATRIX proj = cam.GetProjectionMatrix(1.0f);

    FrustumCuller culler;
    culler.Build(view, proj);
    return culler;
}

// ---- Tests -----------------------------------------------------------------

TEST(FrustumCuller, DefaultNotBuilt_AlwaysVisible)
{
    FrustumCuller culler;
    EXPECT_FALSE(culler.IsBuilt());

    // Unbuilt culler should conservatively report everything as visible.
    BoundingBox anywhere({ 1000.0f, 1000.0f, 1000.0f }, { 1.0f, 1.0f, 1.0f });
    EXPECT_TRUE(culler.IsVisible(anywhere));
}

TEST(FrustumCuller, AABBAtOrigin_IsVisible)
{
    FrustumCuller culler = BuildTestFrustum();
    ASSERT_TRUE(culler.IsBuilt());

    // A box right in front of the camera.
    BoundingBox box({ 0.0f, 0.0f, 2.0f }, { 0.5f, 0.5f, 0.5f });
    EXPECT_TRUE(culler.IsVisible(box));
}

TEST(FrustumCuller, AABBBehindCamera_NotVisible)
{
    FrustumCuller culler = BuildTestFrustum();

    // Behind the camera (camera is at z=-5, looking toward z>-5).
    // An object at z=-50 is behind the camera's near plane.
    BoundingBox box({ 0.0f, 0.0f, -50.0f }, { 0.5f, 0.5f, 0.5f });
    EXPECT_FALSE(culler.IsVisible(box));
}

TEST(FrustumCuller, AABBFarToTheSide_NotVisible)
{
    FrustumCuller culler = BuildTestFrustum();

    // 100 units to the right — well outside the 45-degree FOV.
    BoundingBox box({ 100.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f });
    EXPECT_FALSE(culler.IsVisible(box));
}

TEST(FrustumCuller, AABBBeyondFarPlane_NotVisible)
{
    FrustumCuller culler = BuildTestFrustum();

    // Camera at z=-5, far plane at z= -5+100 = 95.  Place box at z=200.
    BoundingBox box({ 0.0f, 0.0f, 200.0f }, { 0.5f, 0.5f, 0.5f });
    EXPECT_FALSE(culler.IsVisible(box));
}

TEST(FrustumCuller, LargeAABBContainingFrustum_IsVisible)
{
    FrustumCuller culler = BuildTestFrustum();

    // A huge box that completely contains the frustum must be visible.
    BoundingBox huge({ 0.0f, 0.0f, 45.0f }, { 500.0f, 500.0f, 500.0f });
    EXPECT_TRUE(culler.IsVisible(huge));
}

TEST(FrustumCuller, AABBOnFrustumEdge_IsVisible)
{
    FrustumCuller culler = BuildTestFrustum();

    // A small box straddling the frustum boundary should still be visible
    // (intersection, not fully outside).
    BoundingBox edge({ 1.0f, 1.0f, 2.0f }, { 0.1f, 0.1f, 0.1f });
    EXPECT_TRUE(culler.IsVisible(edge));
}

TEST(FrustumCuller, RebuildWithNewCamera_UpdatesFrustum)
{
    Camera cam;
    cam.Reset();  // looking down +Z from (0,0,-5)

    XMMATRIX view = cam.GetViewMatrix();
    XMMATRIX proj = cam.GetProjectionMatrix(1.0f);

    FrustumCuller culler;
    culler.Build(view, proj);

    // Object at (0,0,2) should be visible with this camera.
    BoundingBox inView({ 0.0f, 0.0f, 2.0f }, { 0.5f, 0.5f, 0.5f });
    EXPECT_TRUE(culler.IsVisible(inView));

    // Now rebuild with a camera looking the other way (set manually).
    // We'll use a camera positioned far away looking at the origin,
    // so (0,0,2) is behind the new camera's look direction.
    XMMATRIX view2 = XMMatrixLookAtLH(
        XMVectorSet(0, 0, 50, 1),   // camera at z=+50
        XMVectorSet(0, 0, 100, 1),  // looking toward z=+100
        XMVectorSet(0, 1, 0, 0));
    culler.Build(view2, proj);

    // Now (0,0,2) should be behind the new camera.
    EXPECT_FALSE(culler.IsVisible(inView));
}
