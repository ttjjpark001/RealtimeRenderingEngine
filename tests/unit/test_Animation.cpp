// test_Animation.cpp — Unit tests for Phase 34 Part A: Node Transform Animation
//
// Tests cover CPU-side logic (no GPU required):
//   1. Float3 interpolation  — Linear, Step, boundary conditions, empty input
//   2. Quaternion interpolation — Slerp, Step
//   3. AnimationController  — time advance, looping, pause, transform application

#include <gtest/gtest.h>
#include "Asset/Animation.h"
#include "Core/AnimationController.h"
#include "Scene/SceneNode.h"
#include <DirectXMath.h>
#include <cmath>

using namespace RRE;
using namespace DirectX;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool NearEq(float a, float b, float eps = 1e-4f)
{
    return std::fabsf(a - b) <= eps;
}

static bool NearEqF3(const XMFLOAT3& a, const XMFLOAT3& b, float eps = 1e-4f)
{
    return NearEq(a.x, b.x, eps) && NearEq(a.y, b.y, eps) && NearEq(a.z, b.z, eps);
}

static bool NearEqF4(const XMFLOAT4& a, const XMFLOAT4& b, float eps = 1e-4f)
{
    return NearEq(a.x, b.x, eps) && NearEq(a.y, b.y, eps) &&
           NearEq(a.z, b.z, eps) && NearEq(a.w, b.w, eps);
}

// Build a simple two-keyframe Float3 channel
static std::vector<Keyframe<XMFLOAT3>> MakeF3Keys(
    float t0, XMFLOAT3 v0, float t1, XMFLOAT3 v1)
{
    return { {t0, v0}, {t1, v1} };
}

// ---------------------------------------------------------------------------
// InterpolateFloat3 tests
// ---------------------------------------------------------------------------

TEST(InterpolateFloat3, Linear_Midpoint)
{
    auto keys = MakeF3Keys(0.0f, {0,0,0}, 2.0f, {4,0,0});
    XMFLOAT3 r = InterpolateFloat3(keys, 1.0f, AnimationInterpolation::Linear);
    EXPECT_TRUE(NearEqF3(r, {2,0,0}));
}

TEST(InterpolateFloat3, Linear_QuarterPoint)
{
    auto keys = MakeF3Keys(0.0f, {0,0,0}, 4.0f, {8,0,0});
    XMFLOAT3 r = InterpolateFloat3(keys, 1.0f, AnimationInterpolation::Linear);
    EXPECT_TRUE(NearEqF3(r, {2,0,0}));
}

TEST(InterpolateFloat3, Step_SnapsToLower)
{
    auto keys = MakeF3Keys(0.0f, {1,0,0}, 1.0f, {5,0,0});
    XMFLOAT3 r = InterpolateFloat3(keys, 0.5f, AnimationInterpolation::Step);
    EXPECT_TRUE(NearEqF3(r, {1,0,0}));  // Should stay at t=0 value
}

TEST(InterpolateFloat3, Step_AtBoundary)
{
    auto keys = MakeF3Keys(0.0f, {1,0,0}, 1.0f, {5,0,0});
    XMFLOAT3 r = InterpolateFloat3(keys, 1.0f, AnimationInterpolation::Step);
    EXPECT_TRUE(NearEqF3(r, {5,0,0}));  // Exactly at second key
}

TEST(InterpolateFloat3, BeforeFirst_ReturnsFirst)
{
    auto keys = MakeF3Keys(1.0f, {3,0,0}, 2.0f, {7,0,0});
    XMFLOAT3 r = InterpolateFloat3(keys, 0.0f, AnimationInterpolation::Linear);
    EXPECT_TRUE(NearEqF3(r, {3,0,0}));
}

TEST(InterpolateFloat3, AfterLast_ReturnsLast)
{
    auto keys = MakeF3Keys(0.0f, {1,0,0}, 1.0f, {9,0,0});
    XMFLOAT3 r = InterpolateFloat3(keys, 5.0f, AnimationInterpolation::Linear);
    EXPECT_TRUE(NearEqF3(r, {9,0,0}));
}

TEST(InterpolateFloat3, SingleKey_ReturnsKey)
{
    std::vector<Keyframe<XMFLOAT3>> keys = { {0.5f, {7,0,0}} };
    XMFLOAT3 r = InterpolateFloat3(keys, 0.0f, AnimationInterpolation::Linear);
    EXPECT_TRUE(NearEqF3(r, {7,0,0}));
}

TEST(InterpolateFloat3, Empty_ReturnsZero)
{
    std::vector<Keyframe<XMFLOAT3>> keys;
    XMFLOAT3 r = InterpolateFloat3(keys, 0.5f, AnimationInterpolation::Linear);
    EXPECT_TRUE(NearEqF3(r, {0,0,0}));
}

// ---------------------------------------------------------------------------
// InterpolateQuaternion tests
// ---------------------------------------------------------------------------

TEST(InterpolateQuaternion, Linear_Identity_AtStart)
{
    XMFLOAT4 q0 = {0,0,0,1};  // identity
    XMVECTOR q90v = XMQuaternionRotationAxis(XMVectorSet(0,1,0,0), XM_PIDIV2);
    XMFLOAT4 q90;  XMStoreFloat4(&q90, q90v);

    std::vector<Keyframe<XMFLOAT4>> keys = { {0.0f, q0}, {1.0f, q90} };
    XMFLOAT4 r = InterpolateQuaternion(keys, 0.0f, AnimationInterpolation::Linear);
    EXPECT_TRUE(NearEqF4(r, q0));
}

TEST(InterpolateQuaternion, Linear_Identity_AtEnd)
{
    XMFLOAT4 q0 = {0,0,0,1};
    XMVECTOR q90v = XMQuaternionRotationAxis(XMVectorSet(0,1,0,0), XM_PIDIV2);
    XMFLOAT4 q90;  XMStoreFloat4(&q90, q90v);

    std::vector<Keyframe<XMFLOAT4>> keys = { {0.0f, q0}, {1.0f, q90} };
    XMFLOAT4 r = InterpolateQuaternion(keys, 1.0f, AnimationInterpolation::Linear);
    EXPECT_TRUE(NearEqF4(r, q90));
}

TEST(InterpolateQuaternion, Step_SnapsToLower)
{
    XMFLOAT4 q0 = {0,0,0,1};
    XMVECTOR q90v = XMQuaternionRotationAxis(XMVectorSet(0,1,0,0), XM_PIDIV2);
    XMFLOAT4 q90;  XMStoreFloat4(&q90, q90v);

    std::vector<Keyframe<XMFLOAT4>> keys = { {0.0f, q0}, {1.0f, q90} };
    XMFLOAT4 r = InterpolateQuaternion(keys, 0.5f, AnimationInterpolation::Step);
    EXPECT_TRUE(NearEqF4(r, q0));
}

TEST(InterpolateQuaternion, Empty_ReturnsIdentity)
{
    std::vector<Keyframe<XMFLOAT4>> keys;
    XMFLOAT4 r = InterpolateQuaternion(keys, 0.5f, AnimationInterpolation::Linear);
    EXPECT_TRUE(NearEqF4(r, {0,0,0,1}));
}

// ---------------------------------------------------------------------------
// AnimationController tests
// ---------------------------------------------------------------------------

TEST(AnimationController, Update_AdvancesTime)
{
    SceneNode node;

    AnimationClip clip;
    clip.name     = "TestClip";
    clip.duration = 5.0f;

    AnimationChannel ch;
    ch.targetNode    = &node;
    ch.property      = AnimationProperty::Translation;
    ch.interpolation = AnimationInterpolation::Linear;
    ch.posKeys.push_back({0.0f, {0,0,0}});
    ch.posKeys.push_back({5.0f, {5,0,0}});
    clip.channels.push_back(ch);

    AnimationController ctrl;
    ctrl.SetClip(&clip);
    ctrl.Play();
    ctrl.Update(2.0f);

    EXPECT_NEAR(ctrl.GetCurrentTime(), 2.0f, 1e-4f);
}

TEST(AnimationController, Update_Loops)
{
    SceneNode node;

    AnimationClip clip;
    clip.name     = "LoopClip";
    clip.duration = 2.0f;

    AnimationChannel ch;
    ch.targetNode    = &node;
    ch.property      = AnimationProperty::Translation;
    ch.interpolation = AnimationInterpolation::Linear;
    ch.posKeys.push_back({0.0f, {0,0,0}});
    ch.posKeys.push_back({2.0f, {2,0,0}});
    clip.channels.push_back(ch);

    AnimationController ctrl;
    ctrl.SetClip(&clip);
    ctrl.Play();
    ctrl.Update(2.5f);  // 0.5 past duration → should wrap to 0.5

    EXPECT_NEAR(ctrl.GetCurrentTime(), 0.5f, 1e-4f);
}

TEST(AnimationController, Pause_StopsTime)
{
    SceneNode node;

    AnimationClip clip;
    clip.name     = "PauseClip";
    clip.duration = 10.0f;

    AnimationChannel ch;
    ch.targetNode    = &node;
    ch.property      = AnimationProperty::Translation;
    ch.interpolation = AnimationInterpolation::Linear;
    ch.posKeys.push_back({0.0f, {0,0,0}});
    ch.posKeys.push_back({10.0f, {10,0,0}});
    clip.channels.push_back(ch);

    AnimationController ctrl;
    ctrl.SetClip(&clip);
    ctrl.Play();
    ctrl.Update(1.0f);
    ctrl.Pause();
    ctrl.Update(2.0f);  // Should not advance while paused

    EXPECT_NEAR(ctrl.GetCurrentTime(), 1.0f, 1e-4f);
}

TEST(AnimationController, SetClip_ResetsTime)
{
    SceneNode node;

    AnimationClip clip;
    clip.name     = "ResetClip";
    clip.duration = 5.0f;

    AnimationChannel ch;
    ch.targetNode    = &node;
    ch.property      = AnimationProperty::Translation;
    ch.interpolation = AnimationInterpolation::Linear;
    ch.posKeys.push_back({0.0f, {0,0,0}});
    ch.posKeys.push_back({5.0f, {5,0,0}});
    clip.channels.push_back(ch);

    AnimationController ctrl;
    ctrl.SetClip(&clip);
    ctrl.Play();
    ctrl.Update(3.0f);
    EXPECT_NEAR(ctrl.GetCurrentTime(), 3.0f, 1e-4f);

    ctrl.SetClip(&clip);  // Re-set should reset time
    EXPECT_NEAR(ctrl.GetCurrentTime(), 0.0f, 1e-4f);
}

TEST(AnimationController, Update_NotPlaying_NoTimeAdvance)
{
    SceneNode node;

    AnimationClip clip;
    clip.name     = "NotPlaying";
    clip.duration = 5.0f;

    AnimationChannel ch;
    ch.targetNode    = &node;
    ch.property      = AnimationProperty::Translation;
    ch.interpolation = AnimationInterpolation::Linear;
    ch.posKeys.push_back({0.0f, {0,0,0}});
    ch.posKeys.push_back({5.0f, {5,0,0}});
    clip.channels.push_back(ch);

    AnimationController ctrl;
    ctrl.SetClip(&clip);
    // Do NOT call Play()
    ctrl.Update(2.0f);

    EXPECT_NEAR(ctrl.GetCurrentTime(), 0.0f, 1e-4f);
    EXPECT_FALSE(ctrl.IsPlaying());
}

TEST(AnimationController, Update_AppliesTranslation_ToNode)
{
    SceneNode node;

    AnimationClip clip;
    clip.name     = "TranslateClip";
    clip.duration = 2.0f;

    AnimationChannel ch;
    ch.targetNode    = &node;
    ch.property      = AnimationProperty::Translation;
    ch.interpolation = AnimationInterpolation::Linear;
    ch.posKeys.push_back({0.0f, {0,0,0}});
    ch.posKeys.push_back({2.0f, {4,0,0}});
    clip.channels.push_back(ch);

    AnimationController ctrl;
    ctrl.SetClip(&clip);
    ctrl.Play();
    ctrl.Update(1.0f);  // Midpoint → translation should be {2,0,0}

    XMMATRIX mat = node.GetTransform().GetLocalMatrix();
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, mat);

    // In DirectXMath row-major TRS, translation is in row 4: _41, _42, _43
    EXPECT_NEAR(m._41, 2.0f, 1e-3f);
    EXPECT_NEAR(m._42, 0.0f, 1e-3f);
    EXPECT_NEAR(m._43, 0.0f, 1e-3f);
}

TEST(AnimationController, Update_AppliesScale_ToNode)
{
    SceneNode node;

    AnimationClip clip;
    clip.name     = "ScaleClip";
    clip.duration = 2.0f;

    AnimationChannel ch;
    ch.targetNode    = &node;
    ch.property      = AnimationProperty::Scale;
    ch.interpolation = AnimationInterpolation::Linear;
    ch.posKeys.push_back({0.0f, {1,1,1}});
    ch.posKeys.push_back({2.0f, {3,3,3}});
    clip.channels.push_back(ch);

    AnimationController ctrl;
    ctrl.SetClip(&clip);
    ctrl.Play();
    ctrl.Update(1.0f);  // Midpoint → scale {2,2,2}

    XMMATRIX mat = node.GetTransform().GetLocalMatrix();
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, mat);

    // Uniform scale 2 → diagonal elements _11=_22=_33=2
    EXPECT_NEAR(m._11, 2.0f, 1e-3f);
    EXPECT_NEAR(m._22, 2.0f, 1e-3f);
    EXPECT_NEAR(m._33, 2.0f, 1e-3f);
}

TEST(AnimationController, PlaybackSpeed_Doubles)
{
    SceneNode node;

    AnimationClip clip;
    clip.name     = "SpeedClip";
    clip.duration = 10.0f;

    AnimationChannel ch;
    ch.targetNode    = &node;
    ch.property      = AnimationProperty::Translation;
    ch.interpolation = AnimationInterpolation::Linear;
    ch.posKeys.push_back({0.0f, {0,0,0}});
    ch.posKeys.push_back({10.0f, {10,0,0}});
    clip.channels.push_back(ch);

    AnimationController ctrl;
    ctrl.SetClip(&clip);
    ctrl.SetPlaybackSpeed(2.0f);
    ctrl.Play();
    ctrl.Update(1.0f);

    // dt=1 * speed=2 → currentTime should be 2.0
    EXPECT_NEAR(ctrl.GetCurrentTime(), 2.0f, 1e-4f);
}

TEST(AnimationController, Stop_ResetsTime)
{
    SceneNode node;

    AnimationClip clip;
    clip.name     = "StopClip";
    clip.duration = 5.0f;

    AnimationChannel ch;
    ch.targetNode    = &node;
    ch.property      = AnimationProperty::Translation;
    ch.interpolation = AnimationInterpolation::Linear;
    ch.posKeys.push_back({0.0f, {0,0,0}});
    ch.posKeys.push_back({5.0f, {5,0,0}});
    clip.channels.push_back(ch);

    AnimationController ctrl;
    ctrl.SetClip(&clip);
    ctrl.Play();
    ctrl.Update(3.0f);
    ctrl.Stop();

    EXPECT_NEAR(ctrl.GetCurrentTime(), 0.0f, 1e-4f);
    EXPECT_FALSE(ctrl.IsPlaying());
}
