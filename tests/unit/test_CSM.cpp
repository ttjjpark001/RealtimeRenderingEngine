// test_CSM.cpp — Unit tests for Phase 33a: Cascaded Shadow Maps
//
// Tests cover CPU-side logic that can run without a GPU:
//   1. Cascade split depth calculation (Practical Split Scheme)
//   2. Cascade index selection (mirrors HLSL GetCascadeIndex)
//   3. ShadowConstants struct layout (C++ ↔ HLSL alignment)

#include <gtest/gtest.h>
#include <cmath>
#include "RHI/D3D12/D3D12Context.h"
#include "Renderer/Renderer.h"

using namespace RRE;
using namespace DirectX;

// ---------------------------------------------------------------------------
// Helpers — mirror Renderer.cpp's CSM logic in pure C++ for testing
// ---------------------------------------------------------------------------

static constexpr uint32 NUM_CASCADES = Renderer::CSM_NUM_CASCADES;

// Replicates Renderer::RenderScene cascade split calculation.
static void CalcCascadeSplits(float nearZ, float farZ, float lambda,
                               float outSplits[], uint32 numCascades)
{
    outSplits[0]           = nearZ;
    outSplits[numCascades] = farZ;
    for (uint32 k = 1; k < numCascades; k++)
    {
        float p         = static_cast<float>(k) / numCascades;
        float logSplit  = nearZ * powf(farZ / nearZ, p);
        float unifSplit = nearZ + (farZ - nearZ) * p;
        outSplits[k]    = lambda * logSplit + (1.0f - lambda) * unifSplit;
    }
}

// Mirrors HLSL GetCascadeIndex: split0/split1 = cascadeSplitDepths.x/.y
static int GetCascadeIndex(float viewDepth, float split0, float split1)
{
    if (viewDepth < split0) return 0;
    if (viewDepth < split1) return 1;
    return 2;
}

// ---------------------------------------------------------------------------
// Cascade split depth tests
// ---------------------------------------------------------------------------

TEST(CSM_SplitDepths, MonotonicallyIncreasing)
{
    float splits[NUM_CASCADES + 1];
    CalcCascadeSplits(0.1f, 100.0f, 0.5f, splits, NUM_CASCADES);

    for (uint32 i = 0; i < NUM_CASCADES; i++)
        EXPECT_LT(splits[i], splits[i + 1])
            << "splits[" << i << "] >= splits[" << i + 1 << "]";
}

TEST(CSM_SplitDepths, BoundedByNearAndFar)
{
    const float nearZ = 0.1f, farZ = 100.0f;
    float splits[NUM_CASCADES + 1];
    CalcCascadeSplits(nearZ, farZ, 0.5f, splits, NUM_CASCADES);

    EXPECT_FLOAT_EQ(splits[0], nearZ);
    EXPECT_FLOAT_EQ(splits[NUM_CASCADES], farZ);
    for (uint32 i = 1; i < NUM_CASCADES; i++)
    {
        EXPECT_GT(splits[i], nearZ) << "split[" << i << "] <= nearZ";
        EXPECT_LT(splits[i], farZ)  << "split[" << i << "] >= farZ";
    }
}

TEST(CSM_SplitDepths, Lambda0_PureUniform)
{
    // formula: lambda*log + (1-lambda)*uniform → lambda=0 → pure uniform
    const float nearZ = 0.1f, farZ = 100.0f;
    float splits[NUM_CASCADES + 1];
    CalcCascadeSplits(nearZ, farZ, 0.0f, splits, NUM_CASCADES);

    for (uint32 k = 1; k < NUM_CASCADES; k++)
    {
        float p        = static_cast<float>(k) / NUM_CASCADES;
        float expected = nearZ + (farZ - nearZ) * p;
        EXPECT_NEAR(splits[k], expected, 1e-3f) << "k=" << k;
    }
}

TEST(CSM_SplitDepths, Lambda1_PureLogarithmic)
{
    // formula: lambda*log + (1-lambda)*uniform → lambda=1 → pure logarithmic
    const float nearZ = 0.1f, farZ = 100.0f;
    float splits[NUM_CASCADES + 1];
    CalcCascadeSplits(nearZ, farZ, 1.0f, splits, NUM_CASCADES);

    for (uint32 k = 1; k < NUM_CASCADES; k++)
    {
        float p        = static_cast<float>(k) / NUM_CASCADES;
        float expected = nearZ * powf(farZ / nearZ, p);
        EXPECT_NEAR(splits[k], expected, 1e-3f) << "k=" << k;
    }
}

TEST(CSM_SplitDepths, Sponza_ReasonableRanges)
{
    // Sponza diagonal ~37m → farZ = min(100, 37*3) = 100
    const float nearZ = 0.1f, farZ = 100.0f;
    float splits[NUM_CASCADES + 1];
    CalcCascadeSplits(nearZ, farZ, 0.5f, splits, NUM_CASCADES);

    // cascade 0 should cover close-range geometry (< 25m)
    EXPECT_LT(splits[1], 25.0f) << "cascade 0 far split unexpectedly large";
    // cascade 2 should start well before farZ
    EXPECT_LT(splits[2], farZ) << "cascade 1 far split should be < farZ";
}

// ---------------------------------------------------------------------------
// Cascade index selection tests (mirrors HLSL GetCascadeIndex)
// ---------------------------------------------------------------------------

TEST(CSM_CascadeIndex, NearDepth_IsZero)
{
    // split0=17, split1=38 (typical for Sponza)
    EXPECT_EQ(GetCascadeIndex(0.1f,  17.0f, 38.0f), 0);
    EXPECT_EQ(GetCascadeIndex(10.0f, 17.0f, 38.0f), 0);
    EXPECT_EQ(GetCascadeIndex(16.99f, 17.0f, 38.0f), 0);
}

TEST(CSM_CascadeIndex, MidDepth_IsOne)
{
    EXPECT_EQ(GetCascadeIndex(17.0f, 17.0f, 38.0f), 1);
    EXPECT_EQ(GetCascadeIndex(25.0f, 17.0f, 38.0f), 1);
    EXPECT_EQ(GetCascadeIndex(37.99f, 17.0f, 38.0f), 1);
}

TEST(CSM_CascadeIndex, FarDepth_IsTwo)
{
    EXPECT_EQ(GetCascadeIndex(38.0f,  17.0f, 38.0f), 2);
    EXPECT_EQ(GetCascadeIndex(75.0f,  17.0f, 38.0f), 2);
    EXPECT_EQ(GetCascadeIndex(999.0f, 17.0f, 38.0f), 2);
}

TEST(CSM_CascadeIndex, ExactSplitBoundary_BelongsToHigherCascade)
{
    // At exactly split0, depth is NOT < split0 → cascade 1 (not 0)
    EXPECT_EQ(GetCascadeIndex(17.0f, 17.0f, 38.0f), 1);
    // At exactly split1, depth is NOT < split1 → cascade 2 (not 1)
    EXPECT_EQ(GetCascadeIndex(38.0f, 17.0f, 38.0f), 2);
}

TEST(CSM_CascadeIndex, EqualSplits_StillReturnsValidIndex)
{
    // Degenerate case: all splits the same → should return 2 (far)
    EXPECT_EQ(GetCascadeIndex(5.0f, 5.0f, 5.0f), 2);
}

// ---------------------------------------------------------------------------
// ShadowConstants struct layout (C++ ↔ HLSL 16-byte alignment)
// ---------------------------------------------------------------------------

TEST(CSM_ShadowConstants, SizeFitsIn768Bytes)
{
    // static_assert in D3D12Context.h already enforces this at compile time.
    // This test makes the constraint visible in the test report.
    EXPECT_LE(sizeof(ShadowConstants), 768u);
}

TEST(CSM_ShadowConstants, FieldOffsets_MatchHLSLLayout)
{
    // HLSL cbuffer packs scalars into 16-byte rows.
    // Row after lightViewProj[8] (512 bytes):
    //   shadowMapCount(4) shadowTexelSize(4) shadowNormalBiasWorld(4) csmEnabled(4) = 16 bytes
    // Next row:
    //   cascadeSplitDepths(float3=12) csmDebugView(4) = 16 bytes
    // Next row:
    //   cameraForward(float3=12) _padFwd(4) = 16 bytes
    EXPECT_EQ(offsetof(ShadowConstants, shadowMapCount),       512u);
    EXPECT_EQ(offsetof(ShadowConstants, shadowTexelSize),      516u);
    EXPECT_EQ(offsetof(ShadowConstants, shadowNormalBiasWorld),520u);
    EXPECT_EQ(offsetof(ShadowConstants, csmEnabled),           524u);
    EXPECT_EQ(offsetof(ShadowConstants, cascadeSplitDepths),   528u);
    EXPECT_EQ(offsetof(ShadowConstants, csmDebugView),         540u);
    EXPECT_EQ(offsetof(ShadowConstants, cameraForward),        544u);
}

// ---------------------------------------------------------------------------
// Cascade blend band (mirrors PBR.hlsl CalcShadowCSM blend logic)
// ---------------------------------------------------------------------------

// Returns blend factor t in [0,1] for the cascade 0→1 boundary.
// t=0 → pure cascade 0, t=1 → pure cascade 1.
static float BlendFactor(float viewDepth, float splitDepth, float bandWidth)
{
    float start = splitDepth - bandWidth;
    float t = (viewDepth - start) / bandWidth;
    return (std::max)(0.0f, (std::min)(1.0f, t));
}

TEST(CSM_BlendBand, BeforeBand_IsZero)
{
    // viewDepth well before blend zone → pure cascade 0 (t = 0)
    float split = 17.0f, band = split * 0.1f;
    EXPECT_FLOAT_EQ(BlendFactor(split - band - 1.0f, split, band), 0.0f);
}

TEST(CSM_BlendBand, AfterBand_IsOne)
{
    // viewDepth at or past split → pure cascade 1 (t = 1)
    float split = 17.0f, band = split * 0.1f;
    EXPECT_FLOAT_EQ(BlendFactor(split, split, band), 1.0f);
    EXPECT_FLOAT_EQ(BlendFactor(split + 5.0f, split, band), 1.0f);
}

TEST(CSM_BlendBand, InsideBand_IsInRange)
{
    float split = 17.0f, band = split * 0.1f;
    float t = BlendFactor(split - band * 0.5f, split, band);
    EXPECT_GT(t, 0.0f);
    EXPECT_LT(t, 1.0f);
}

TEST(CSM_BlendBand, MidBand_IsApproximatelyHalf)
{
    float split = 17.0f, band = split * 0.1f;
    float t = BlendFactor(split - band * 0.5f, split, band);
    EXPECT_NEAR(t, 0.5f, 1e-5f);
}

TEST(CSM_BlendBand, MonotonicallyIncreasing)
{
    float split = 17.0f, band = split * 0.1f;
    float prev = BlendFactor(split - band - 1.0f, split, band);
    for (float d = split - band; d <= split + 1.0f; d += 0.5f)
    {
        float curr = BlendFactor(d, split, band);
        EXPECT_GE(curr, prev) << "Not monotonic at viewDepth=" << d;
        prev = curr;
    }
}
