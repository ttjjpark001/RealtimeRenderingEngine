#include <gtest/gtest.h>
#include "Scene/Transform.h"
#include "Math/MathUtil.h"

using namespace DirectX;
using namespace RRE;

TEST(Transform, DefaultIsIdentity)
{
    Transform t;
    XMMATRIX local = t.GetLocalMatrix();
    XMFLOAT4X4 mat, identity;
    XMStoreFloat4x4(&mat, local);
    XMStoreFloat4x4(&identity, XMMatrixIdentity());

    EXPECT_TRUE(Math::NearEqualMatrix(mat, identity));
}

TEST(Transform, TranslationOnly)
{
    Transform t;
    t.SetPosition({ 3.0f, 4.0f, 5.0f });
    XMMATRIX local = t.GetLocalMatrix();

    // Transform the origin
    XMVECTOR origin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR result = XMVector4Transform(origin, local);
    XMFLOAT3 pos;
    XMStoreFloat3(&pos, result);

    EXPECT_TRUE(Math::NearEqual(pos.x, 3.0f));
    EXPECT_TRUE(Math::NearEqual(pos.y, 4.0f));
    EXPECT_TRUE(Math::NearEqual(pos.z, 5.0f));
}

TEST(Transform, ScaleOnly)
{
    Transform t;
    t.SetScale({ 2.0f, 3.0f, 4.0f });
    XMMATRIX local = t.GetLocalMatrix();

    XMVECTOR point = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
    XMVECTOR result = XMVector4Transform(point, local);
    XMFLOAT3 pos;
    XMStoreFloat3(&pos, result);

    EXPECT_TRUE(Math::NearEqual(pos.x, 2.0f));
    EXPECT_TRUE(Math::NearEqual(pos.y, 3.0f));
    EXPECT_TRUE(Math::NearEqual(pos.z, 4.0f));
}

TEST(Transform, RotationY90)
{
    Transform t;
    t.SetRotation({ 0.0f, XM_PIDIV2, 0.0f }); // 90 degrees around Y
    XMMATRIX local = t.GetLocalMatrix();

    XMVECTOR point = XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR result = XMVector4Transform(point, local);
    XMFLOAT3 pos;
    XMStoreFloat3(&pos, result);

    // (1,0,0) rotated 90 around Y -> (0,0,-1)
    EXPECT_TRUE(Math::NearEqual(pos.x, 0.0f));
    EXPECT_TRUE(Math::NearEqual(pos.y, 0.0f));
    EXPECT_TRUE(Math::NearEqual(pos.z, -1.0f));
}

TEST(Transform, TRSCombined)
{
    Transform t;
    t.SetPosition({ 10.0f, 0.0f, 0.0f });
    t.SetScale({ 2.0f, 2.0f, 2.0f });
    // No rotation
    XMMATRIX local = t.GetLocalMatrix();

    XMVECTOR point = XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR result = XMVector4Transform(point, local);
    XMFLOAT3 pos;
    XMStoreFloat3(&pos, result);

    // Scale(2) * point(1,0,0) = (2,0,0), then translate(10,0,0) = (12,0,0)
    EXPECT_TRUE(Math::NearEqual(pos.x, 12.0f));
    EXPECT_TRUE(Math::NearEqual(pos.y, 0.0f));
    EXPECT_TRUE(Math::NearEqual(pos.z, 0.0f));
}
