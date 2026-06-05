// test_PCSS.cpp — Unit tests for Phase 33 Part C: PCSS
//
// Tests cover CPU-side logic verifiable without a GPU:
//   1. Penumbra width calculation
//   2. Filter radius clamping
//   3. ShadowConstants struct layout (C++ ↔ HLSL alignment)

#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>
#include "RHI/D3D12/D3D12Context.h"

using namespace RRE;

// ---------------------------------------------------------------------------
// Helpers — mirror PBR.hlsl PCSS logic in pure C++
// ---------------------------------------------------------------------------

static float CalcPenumbraWidth(float receiverDepth, float avgBlockerDepth, float lightSize)
{
    return (receiverDepth - avgBlockerDepth) / avgBlockerDepth * lightSize;
}

static float CalcFilterRadius(float penumbraWidth, float shadowTexelSize)
{
    float raw = penumbraWidth * shadowTexelSize;
    float minR = shadowTexelSize * 1.5f;
    float maxR = shadowTexelSize * 9.0f;
    return (std::max)(minR, (std::min)(maxR, raw));
}

// ---------------------------------------------------------------------------
// Penumbra width tests
// ---------------------------------------------------------------------------

TEST(PCSS_PenumbraWidth, BlockerAtReceiver_IsZero)
{
    // blocker == receiver → no gap → penumbra = 0
    EXPECT_FLOAT_EQ(CalcPenumbraWidth(0.5f, 0.5f, 1.0f), 0.0f);
}

TEST(PCSS_PenumbraWidth, ScalesWithLightSize)
{
    float w1 = CalcPenumbraWidth(0.8f, 0.4f, 1.0f);
    float w2 = CalcPenumbraWidth(0.8f, 0.4f, 2.0f);
    EXPECT_NEAR(w2, w1 * 2.0f, 1e-5f);
}

TEST(PCSS_PenumbraWidth, LargerGap_LargerPenumbra)
{
    // receiver further from blocker → wider penumbra
    float wSmall = CalcPenumbraWidth(0.6f, 0.5f, 1.0f);
    float wLarge = CalcPenumbraWidth(0.9f, 0.5f, 1.0f);
    EXPECT_GT(wLarge, wSmall);
}

TEST(PCSS_PenumbraWidth, BlockerCloserToLight_WiderPenumbra)
{
    // same receiver depth, blocker closer to light (smaller depth) → wider
    float wFar  = CalcPenumbraWidth(0.8f, 0.6f, 1.0f);
    float wNear = CalcPenumbraWidth(0.8f, 0.3f, 1.0f);
    EXPECT_GT(wNear, wFar);
}

TEST(PCSS_PenumbraWidth, AlwaysNonNegative_WhenBlockerBehindReceiver)
{
    // blocker depth < receiver depth → valid occlusion → penumbra >= 0
    float w = CalcPenumbraWidth(0.7f, 0.3f, 1.0f);
    EXPECT_GE(w, 0.0f);
}

// ---------------------------------------------------------------------------
// Filter radius clamping tests
// ---------------------------------------------------------------------------

TEST(PCSS_FilterRadius, ClampedToMinWhenPenumbraIsZero)
{
    float texelSize = 1.0f / 2048.0f;
    float r = CalcFilterRadius(0.0f, texelSize);
    EXPECT_FLOAT_EQ(r, texelSize * 1.5f);
}

TEST(PCSS_FilterRadius, ClampedToMaxWhenPenumbraIsHuge)
{
    float texelSize = 1.0f / 1024.0f;
    float r = CalcFilterRadius(1000.0f, texelSize);
    EXPECT_FLOAT_EQ(r, texelSize * 9.0f);
}

TEST(PCSS_FilterRadius, InRangeWhenPenumbraIsModerate)
{
    float texelSize = 1.0f / 2048.0f;
    float penumbra = 5.0f;
    float r = CalcFilterRadius(penumbra, texelSize);
    EXPECT_GE(r, texelSize * 1.5f);
    EXPECT_LE(r, texelSize * 9.0f);
}

TEST(PCSS_FilterRadius, MonotonicallyIncreasingUntilMax)
{
    float texelSize = 1.0f / 2048.0f;
    float prev = CalcFilterRadius(0.0f, texelSize);
    for (float p = 1.0f; p <= 50.0f; p += 1.0f)
    {
        float curr = CalcFilterRadius(p, texelSize);
        EXPECT_GE(curr, prev) << "Not monotonic at penumbra=" << p;
        prev = curr;
    }
}

// ---------------------------------------------------------------------------
// ShadowConstants layout — pcssEnabled / lightSize offset verification
// ---------------------------------------------------------------------------

TEST(PCSS_ShadowConstants, SizeFitsIn768Bytes)
{
    EXPECT_LE(sizeof(ShadowConstants), 768u);
}

TEST(PCSS_ShadowConstants, FieldOffsets_MatchHLSLLayout)
{
    // After cameraForward(12) + _padFwd(4) at offset 544 → row ends at 560
    // pcssEnabled(4) at 560, lightSize(4) at 564, _padPCSS[2](8) at 568
    EXPECT_EQ(offsetof(ShadowConstants, pcssEnabled), 560u);
    EXPECT_EQ(offsetof(ShadowConstants, lightSize),   564u);
}

TEST(PCSS_ShadowConstants, LightSize_DefaultIsZero)
{
    // Zero-initialized ShadowConstants must have lightSize = 0
    ShadowConstants sc = {};
    EXPECT_FLOAT_EQ(sc.lightSize, 0.0f);
    EXPECT_EQ(sc.pcssEnabled, 0u);
}
