#include <gtest/gtest.h>
#include "Renderer/LightCuller.h"
#include "Renderer/FrustumCuller.h"
#include "Lighting/Light.h"
#include "Lighting/LightManager.h"
#include "Scene/Camera.h"
#include <DirectXMath.h>

using namespace DirectX;
using namespace RRE;

// Helper: build a standard test frustum (camera at origin looking +Z, near=1, far=100)
static DirectX::BoundingFrustum BuildTestFrustum()
{
    XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(0, 0, 0, 1),
        XMVectorSet(0, 0, 1, 1),
        XMVectorSet(0, 1, 0, 0));
    XMMATRIX proj = XMMatrixPerspectiveFovLH(
        XM_PIDIV4, 1.0f, 1.0f, 100.0f);

    FrustumCuller fc;
    fc.Build(view, proj);
    return fc.GetFrustum();
}

// Helper: add a point light at the given position with given intensity
static void AddPointLight(LightManager& lm, XMFLOAT3 pos, float intensity = 1.0f)
{
    Light l;
    l.type      = LightType::Point;
    l.position  = pos;
    l.intensity = intensity;
    l.Kc = 1.0f; l.Kl = 0.09f; l.Kq = 0.032f;
    lm.AddLight(l);
}

// ---- Tests -----------------------------------------------------------------

TEST(LightCuller, EmptyLightManager_ReturnsEmpty)
{
    LightManager lm;
    LightCuller culler;
    auto active = culler.CullLights(BuildTestFrustum(), { 0,0,0 }, lm);
    EXPECT_TRUE(active.empty());
    EXPECT_EQ(culler.GetLastCulledCount(), 0u);
    EXPECT_EQ(culler.GetLastActiveCount(), 0u);
}

TEST(LightCuller, DirectionalLight_AlwaysActive)
{
    LightManager lm;
    Light dir;
    dir.type      = LightType::Directional;
    dir.direction = { 0, -1, 0 };
    dir.intensity = 1.0f;
    lm.AddLight(dir);

    LightCuller culler;
    XMFLOAT3 cam = { 0,0,0 };
    auto active = culler.CullLights(BuildTestFrustum(), cam, lm);

    EXPECT_EQ(active.size(), 1u);
    EXPECT_EQ(active[0], 0u);
    EXPECT_EQ(culler.GetLastCulledCount(), 0u);
}

TEST(LightCuller, PointLightInFrustum_IsActive)
{
    LightManager lm;
    // Light inside the frustum (camera at origin looking +Z, light at z=10)
    AddPointLight(lm, { 0.0f, 0.0f, 10.0f }, 10.0f);

    LightCuller culler;
    XMFLOAT3 cam = { 0, 0, 0 };
    auto active = culler.CullLights(BuildTestFrustum(), cam, lm);

    EXPECT_EQ(active.size(), 1u);
}

TEST(LightCuller, PointLightBehindCamera_IsCulled)
{
    LightManager lm;
    // Light 200 units behind the camera
    AddPointLight(lm, { 0.0f, 0.0f, -200.0f }, 1.0f);

    LightCuller culler;
    XMFLOAT3 cam = { 0, 0, 0 };
    auto active = culler.CullLights(BuildTestFrustum(), cam, lm);

    EXPECT_TRUE(active.empty());
    EXPECT_EQ(culler.GetLastCulledCount(), 1u);
}

TEST(LightCuller, PointLightTinyIntensity_IsCulled)
{
    LightManager lm;
    // Light in the frustum but with negligible intensity at camera distance
    AddPointLight(lm, { 0.0f, 0.0f, 50.0f }, 0.0001f);  // effectively zero contribution

    LightCuller culler;
    XMFLOAT3 cam = { 0, 0, 0 };
    auto active = culler.CullLights(BuildTestFrustum(), cam, lm);

    EXPECT_TRUE(active.empty());
    EXPECT_EQ(culler.GetLastCulledCount(), 1u);
}

TEST(LightCuller, MixedLights_CorrectPartition)
{
    LightManager lm;

    // Index 0: Directional — always active
    Light dir;
    dir.type      = LightType::Directional;
    dir.intensity = 1.0f;
    lm.AddLight(dir);

    // Index 1: Point light inside frustum — active
    AddPointLight(lm, { 0.0f, 0.0f, 5.0f }, 5.0f);

    // Index 2: Point light far outside frustum — culled
    AddPointLight(lm, { 500.0f, 0.0f, 0.0f }, 1.0f);

    LightCuller culler;
    XMFLOAT3 cam = { 0, 0, 0 };
    auto active = culler.CullLights(BuildTestFrustum(), cam, lm);

    EXPECT_EQ(active.size(), 2u);    // directional + near point
    EXPECT_EQ(culler.GetLastCulledCount(), 1u);   // only the far point light
}

TEST(LightCuller, StatsAreUpdatedPerCall)
{
    LightManager lm;
    AddPointLight(lm, { 0.0f, 0.0f, 5.0f }, 5.0f);

    LightCuller culler;
    XMFLOAT3 cam = { 0, 0, 0 };

    auto a1 = culler.CullLights(BuildTestFrustum(), cam, lm);
    uint32_t active1 = culler.GetLastActiveCount();

    // Move camera far away so the light is culled
    XMFLOAT3 farCam = { 0, 0, -9999.0f };
    auto a2 = culler.CullLights(BuildTestFrustum(), farCam, lm);

    // Stats must reflect the second call
    EXPECT_EQ(culler.GetLastActiveCount() + culler.GetLastCulledCount(), 1u);
}
