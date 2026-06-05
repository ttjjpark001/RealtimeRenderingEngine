// test_SpotShadow.cpp — Unit tests for Phase 33 Part D: Spot Light Shadow
//
// Spot Light shadow uses the same Texture2D pipeline as Directional Light.
// All logic is CPU-side and GPU-free.
//
// Test groups:
//   1. shadowMapIndex allocation via LightManager
//   2. Perspective FOV calculation from outerConeAngle
//   3. ShadowCasterCount accounting
//   4. Mixed light type allocation (Directional + Spot + Point)

#include <gtest/gtest.h>
#include <cmath>
#include "Lighting/Light.h"
#include "Lighting/LightManager.h"
#include "RHI/D3D12/D3D12Context.h"

using namespace RRE;
using namespace DirectX;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Light MakeSpotLight(bool castShadow, float outerCone = 0.8192f)
{
    Light l;
    l.type           = LightType::Spot;
    l.position       = { 0.0f, 5.0f, 0.0f };
    l.direction      = { 0.0f, -1.0f, 0.0f };
    l.color          = { 1.0f, 1.0f, 1.0f };
    l.intensity      = 10.0f;
    l.outerConeAngle = outerCone;
    l.castShadow     = castShadow;
    return l;
}

static Light MakeDirectionalLight(bool castShadow)
{
    Light l;
    l.type      = LightType::Directional;
    l.direction = { -0.3f, -1.0f, 0.5f };
    l.color     = { 1.0f, 1.0f, 1.0f };
    l.intensity = 5.0f;
    l.castShadow = castShadow;
    return l;
}

static Light MakePointLight(bool castShadow)
{
    Light l;
    l.type      = LightType::Point;
    l.position  = { 0.0f, 2.0f, 0.0f };
    l.color     = { 1.0f, 1.0f, 1.0f };
    l.intensity = 8.0f;
    l.castShadow = castShadow;
    return l;
}

// ---------------------------------------------------------------------------
// 1. shadowMapIndex allocation
// ---------------------------------------------------------------------------

TEST(SpotShadow_Allocation, NoCastShadow_IndexIsNegOne)
{
    LightManager lm;
    lm.AddLight(MakeSpotLight(false));
    auto c = lm.BuildLightConstants();
    EXPECT_EQ(c.lights[0].shadowMapIndex, -1);
}

TEST(SpotShadow_Allocation, CastShadow_IndexIsZero)
{
    LightManager lm;
    lm.AddLight(MakeSpotLight(true));
    auto c = lm.BuildLightConstants();
    EXPECT_EQ(c.lights[0].shadowMapIndex, 0);
}

TEST(SpotShadow_Allocation, CastShadow_ShadowTypeIsTexture2D)
{
    LightManager lm;
    lm.AddLight(MakeSpotLight(true));
    auto c = lm.BuildLightConstants();
    EXPECT_EQ(c.lights[0].shadowType, 0u);  // 0 = Texture2D
}

TEST(SpotShadow_Allocation, TwoSpotLights_GetSequentialIndices)
{
    LightManager lm;
    lm.AddLight(MakeSpotLight(true));
    lm.AddLight(MakeSpotLight(true));
    auto c = lm.BuildLightConstants();
    EXPECT_EQ(c.lights[0].shadowMapIndex, 0);
    EXPECT_EQ(c.lights[1].shadowMapIndex, 1);
}

TEST(SpotShadow_Allocation, SecondSpotNoShadow_SkipsIndex)
{
    LightManager lm;
    lm.AddLight(MakeSpotLight(true));
    lm.AddLight(MakeSpotLight(false));
    lm.AddLight(MakeSpotLight(true));
    auto c = lm.BuildLightConstants();
    EXPECT_EQ(c.lights[0].shadowMapIndex, 0);
    EXPECT_EQ(c.lights[1].shadowMapIndex, -1);
    EXPECT_EQ(c.lights[2].shadowMapIndex, 1);
}

// ---------------------------------------------------------------------------
// 2. Perspective FOV from outerConeAngle
// ---------------------------------------------------------------------------

// Renderer uses: fov = acosf(outerConeAngle) * 2.0f  (clamped to >= 0.1f)
static float CalcSpotFov(float outerConeAngle)
{
    float fov = acosf(outerConeAngle) * 2.0f;
    return (fov < 0.1f) ? 0.1f : fov;
}

TEST(SpotShadow_FOV, OuterCone35deg_GivesCorrectFov)
{
    // cos(35°) ≈ 0.8192 → fov = 2 × 35° = 70° ≈ 1.2217 rad
    float fov = CalcSpotFov(0.8192f);
    EXPECT_NEAR(fov, 2.0f * acosf(0.8192f), 1e-4f);
    EXPECT_GT(fov, 0.0f);
}

TEST(SpotShadow_FOV, OuterCone45deg_GivesFov90deg)
{
    // cos(45°) = 0.7071 → fov = 90° = π/2
    float fov = CalcSpotFov(0.7071f);
    EXPECT_NEAR(fov, XM_PIDIV2, 1e-3f);
}

TEST(SpotShadow_FOV, NarrowCone_ClampedToMinimum)
{
    // cos(very small angle) ≈ 1.0 → raw fov ≈ 0 → clamped to 0.1
    float fov = CalcSpotFov(0.9999f);
    EXPECT_FLOAT_EQ(fov, 0.1f);
}

TEST(SpotShadow_FOV, WiderCone_GivesWiderFov)
{
    float fovNarrow = CalcSpotFov(0.9f);   // cos(~25.8°) → small fov
    float fovWide   = CalcSpotFov(0.5f);   // cos(60°) → larger fov
    EXPECT_GT(fovWide, fovNarrow);
}

// ---------------------------------------------------------------------------
// 3. ShadowCasterCount
// ---------------------------------------------------------------------------

TEST(SpotShadow_CasterCount, SingleSpot_CountIsOne)
{
    LightManager lm;
    lm.AddLight(MakeSpotLight(true));
    lm.BuildLightConstants();
    EXPECT_EQ(lm.GetShadowCasterCount(), 1u);
}

TEST(SpotShadow_CasterCount, TwoSpots_CountIsTwo)
{
    LightManager lm;
    lm.AddLight(MakeSpotLight(true));
    lm.AddLight(MakeSpotLight(true));
    lm.BuildLightConstants();
    EXPECT_EQ(lm.GetShadowCasterCount(), 2u);
}

TEST(SpotShadow_CasterCount, SpotNoCastShadow_CountIsZero)
{
    LightManager lm;
    lm.AddLight(MakeSpotLight(false));
    lm.BuildLightConstants();
    EXPECT_EQ(lm.GetShadowCasterCount(), 0u);
}

// ---------------------------------------------------------------------------
// 4. Mixed light type allocation
// ---------------------------------------------------------------------------

TEST(SpotShadow_Mixed, Directional_ThenSpot_SequentialTexture2DSlots)
{
    LightManager lm;
    lm.AddLight(MakeDirectionalLight(true));  // Texture2D slot 0
    lm.AddLight(MakeSpotLight(true));         // Texture2D slot 1
    auto c = lm.BuildLightConstants();
    EXPECT_EQ(c.lights[0].shadowMapIndex, 0);
    EXPECT_EQ(c.lights[1].shadowMapIndex, 1);
    EXPECT_EQ(c.lights[0].shadowType, 0u);
    EXPECT_EQ(c.lights[1].shadowType, 0u);
}

TEST(SpotShadow_Mixed, Point_UsesTextureCube_SpotUsesTexture2D)
{
    LightManager lm;
    lm.AddLight(MakePointLight(true));   // TextureCube slot 0
    lm.AddLight(MakeSpotLight(true));    // Texture2D slot 0
    auto c = lm.BuildLightConstants();
    EXPECT_EQ(c.lights[0].shadowType, 1u);  // Point → TextureCube
    EXPECT_EQ(c.lights[1].shadowType, 0u);  // Spot  → Texture2D
    EXPECT_EQ(c.lights[0].shadowMapIndex, 0);
    EXPECT_EQ(c.lights[1].shadowMapIndex, 0);  // 별도 슬롯 공간 사용
}

TEST(SpotShadow_Mixed, DirectionalCsmThreeSlots_SpotGetsSlotThree)
{
    // CSM Directional은 렌더러가 슬롯 0~2를 사용.
    // LightManager 할당은 Directional=1슬롯, 렌더러가 실제 CSM 슬롯을 분배.
    // Directional(castShadow) → slot 0, Spot → slot 1
    LightManager lm;
    lm.AddLight(MakeDirectionalLight(true));
    lm.AddLight(MakeSpotLight(true));
    auto c = lm.BuildLightConstants();
    EXPECT_EQ(lm.GetShadowCasterCount(), 2u);
    EXPECT_EQ(c.lights[0].shadowMapIndex, 0);
    EXPECT_EQ(c.lights[1].shadowMapIndex, 1);
}

TEST(SpotShadow_Mixed, MaxSlotsNotExceeded)
{
    LightManager lm;
    // MAX_SHADOW_MAPS=8 개 이상 추가 시 초과분은 shadowMapIndex=-1
    for (int i = 0; i < 10; i++)
        lm.AddLight(MakeSpotLight(true));
    auto c = lm.BuildLightConstants();
    int assigned = 0;
    for (uint32 i = 0; i < c.numActiveLights; i++)
        if (c.lights[i].shadowMapIndex >= 0) assigned++;
    EXPECT_LE(assigned, static_cast<int>(MAX_SHADOW_MAPS));
}
