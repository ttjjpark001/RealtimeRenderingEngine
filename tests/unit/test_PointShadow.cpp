// test_PointShadow.cpp — Unit tests for Phase 33 Part A: Point Light Cube Map Shadow
//
// Tests cover CPU-side logic verifiable without a GPU:
//   1. Cube shadow face direction / up vector geometry
//   2. Linear depth normalisation
//   3. LightData struct layout (shadowType field offset)
//   4. CubeShadowPassConstants struct size

#include <gtest/gtest.h>
#include <cmath>
#include "RHI/D3D12/D3D12Context.h"

using namespace RRE;
using namespace DirectX;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static float CalcLinearDepth(float dist, float farPlane)
{
    return dist / farPlane;
}

// Verify two float3 vectors are approximately equal
static bool NearEqual3(XMFLOAT3 a, XMFLOAT3 b, float eps = 1e-5f)
{
    return fabsf(a.x - b.x) < eps && fabsf(a.y - b.y) < eps && fabsf(a.z - b.z) < eps;
}

// ---------------------------------------------------------------------------
// Cube face directions (D3D12 TextureCube ordering: +X,-X,+Y,-Y,+Z,-Z)
// ---------------------------------------------------------------------------

TEST(PointShadow_CubeFace, Directions_AreUnitVectors)
{
    static const XMFLOAT3 dirs[6] = {
        {+1,0,0}, {-1,0,0}, {0,+1,0}, {0,-1,0}, {0,0,+1}, {0,0,-1}
    };
    for (int i = 0; i < 6; i++)
    {
        float len = sqrtf(dirs[i].x*dirs[i].x + dirs[i].y*dirs[i].y + dirs[i].z*dirs[i].z);
        EXPECT_NEAR(len, 1.0f, 1e-6f) << "face " << i;
    }
}

TEST(PointShadow_CubeFace, DirectionsAndUps_AreOrthogonal)
{
    static const XMFLOAT3 dirs[6] = {
        {+1,0,0}, {-1,0,0}, {0,+1,0}, {0,-1,0}, {0,0,+1}, {0,0,-1}
    };
    static const XMFLOAT3 ups[6] = {
        {0,+1,0}, {0,+1,0}, {0,0,-1}, {0,0,+1}, {0,+1,0}, {0,+1,0}
    };
    for (int i = 0; i < 6; i++)
    {
        float dot = dirs[i].x*ups[i].x + dirs[i].y*ups[i].y + dirs[i].z*ups[i].z;
        EXPECT_NEAR(dot, 0.0f, 1e-6f) << "face " << i << " dir/up not orthogonal";
    }
}

TEST(PointShadow_CubeFace, OppositeDirectionsAtIndices_0and1)
{
    // +X face (0) and -X face (1) must point in opposite directions
    XMFLOAT3 pos = {+1,0,0};
    XMFLOAT3 neg = {-1,0,0};
    EXPECT_NEAR(pos.x, -neg.x, 1e-6f);
}

// ---------------------------------------------------------------------------
// Linear depth normalisation
// ---------------------------------------------------------------------------

TEST(PointShadow_LinearDepth, AtFarPlane_IsOne)
{
    EXPECT_FLOAT_EQ(CalcLinearDepth(100.0f, 100.0f), 1.0f);
}

TEST(PointShadow_LinearDepth, AtNearPlane_IsSmall)
{
    float nearZ = 0.1f, farZ = 100.0f;
    float d = CalcLinearDepth(nearZ, farZ);
    EXPECT_GT(d, 0.0f);
    EXPECT_LT(d, 0.01f);
}

TEST(PointShadow_LinearDepth, ScalesLinearly)
{
    float farZ = 100.0f;
    float d1 = CalcLinearDepth(10.0f, farZ);
    float d2 = CalcLinearDepth(20.0f, farZ);
    EXPECT_NEAR(d2, d1 * 2.0f, 1e-5f);
}

TEST(PointShadow_LinearDepth, BeyondFarPlane_IsAboveOne)
{
    // fragment beyond far plane → normalised depth > 1 → won't be shadowed (bias handles this)
    float d = CalcLinearDepth(150.0f, 100.0f);
    EXPECT_GT(d, 1.0f);
}

// ---------------------------------------------------------------------------
// LightData struct layout — shadowType field
// ---------------------------------------------------------------------------

TEST(PointShadow_LightData, ShadowType_FieldExists)
{
    GPULightData light = {};
    light.shadowType = 1;
    EXPECT_EQ(light.shadowType, 1u);
    light.shadowType = 0;
    EXPECT_EQ(light.shadowType, 0u);
}

TEST(PointShadow_LightData, SizeUnchanged_Is80Bytes)
{
    EXPECT_EQ(sizeof(GPULightData), 80u);
}

// ---------------------------------------------------------------------------
// CubeShadowPassConstants struct
// ---------------------------------------------------------------------------

TEST(PointShadow_CubeShadowPassConstants, SizeFitsIn256Bytes)
{
    EXPECT_LE(sizeof(CubeShadowPassConstants), 256u);
}

TEST(PointShadow_CubeShadowPassConstants, LightPosAndFarPlane_AtExpectedOffsets)
{
    // lightViewProj (64 bytes) → lightPos at offset 64, farPlane at offset 76
    EXPECT_EQ(offsetof(CubeShadowPassConstants, lightPos),   64u);
    EXPECT_EQ(offsetof(CubeShadowPassConstants, farPlane),   76u);
}

// ---------------------------------------------------------------------------
// D3D12Context constants
// ---------------------------------------------------------------------------

TEST(PointShadow_Constants, MaxPointShadowLights_IsFour)
{
    EXPECT_EQ(D3D12Context::MAX_POINT_SHADOW_LIGHTS, 4u);
}

TEST(PointShadow_Constants, CubeFaces_IsSix)
{
    EXPECT_EQ(D3D12Context::CUBE_FACES, 6u);
}
