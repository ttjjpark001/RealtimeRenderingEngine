#include <gtest/gtest.h>
#include "Math/MathUtil.h"

using namespace DirectX;
using namespace RRE::Math;

// Identity matrix * vector = original vector
TEST(MathUtil, IdentityMatrixPreservesVector)
{
    XMFLOAT3 v(1.0f, 2.0f, 3.0f);
    XMMATRIX identity = XMMatrixIdentity();
    XMVECTOR vVec = LoadVector3(v);
    XMVECTOR result = XMVector3Transform(vVec, identity);
    XMFLOAT3 resultStored = StoreVector3(result);

    EXPECT_TRUE(NearEqualVector3(v, resultStored));
}

// RotationY(pi/2) transforms (1,0,0) to (0,0,-1)
TEST(MathUtil, RotationYTransformsVector)
{
    XMFLOAT3 v(1.0f, 0.0f, 0.0f);
    XMMATRIX rotY = XMMatrixRotationY(XM_PIDIV2);
    XMVECTOR vVec = LoadVector3(v);
    XMVECTOR result = XMVector3Transform(vVec, rotY);
    XMFLOAT3 resultStored = StoreVector3(result);

    XMFLOAT3 expected(0.0f, 0.0f, -1.0f);
    EXPECT_TRUE(NearEqualVector3(resultStored, expected, 1e-4f));
}

// TRS matrix with Translation(1,2,3) + no rotation + Scale(1,1,1) => position (1,2,3)
TEST(MathUtil, TRSMatrixTranslationOnly)
{
    XMFLOAT3 position(1.0f, 2.0f, 3.0f);
    XMFLOAT3 rotation(0.0f, 0.0f, 0.0f);
    XMFLOAT3 scale(1.0f, 1.0f, 1.0f);

    XMMATRIX trs = CreateTRSMatrix(position, rotation, scale);

    // Transform origin point
    XMVECTOR origin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR result = XMVector4Transform(origin, trs);
    XMFLOAT4 resultStored;
    XMStoreFloat4(&resultStored, result);

    EXPECT_TRUE(NearEqual(resultStored.x, 1.0f));
    EXPECT_TRUE(NearEqual(resultStored.y, 2.0f));
    EXPECT_TRUE(NearEqual(resultStored.z, 3.0f));
}

// Matrix multiplication associativity: (A*B)*C = A*(B*C)
TEST(MathUtil, MatrixMultiplicationAssociativity)
{
    XMMATRIX A = XMMatrixRotationX(0.5f);
    XMMATRIX B = XMMatrixTranslation(1.0f, 2.0f, 3.0f);
    XMMATRIX C = XMMatrixScaling(2.0f, 2.0f, 2.0f);

    XMMATRIX AB_C = (A * B) * C;
    XMMATRIX A_BC = A * (B * C);

    XMFLOAT4X4 ab_c, a_bc;
    XMStoreFloat4x4(&ab_c, AB_C);
    XMStoreFloat4x4(&a_bc, A_BC);

    EXPECT_TRUE(NearEqualMatrix(ab_c, a_bc, 1e-4f));
}

// NearEqual utility
TEST(MathUtil, NearEqualBasic)
{
    EXPECT_TRUE(NearEqual(1.0f, 1.0f));
    EXPECT_TRUE(NearEqual(1.0f, 1.000001f));
    EXPECT_FALSE(NearEqual(1.0f, 1.1f));
    EXPECT_TRUE(NearEqual(0.0f, 0.0f));
    EXPECT_FALSE(NearEqual(0.0f, 1.0f));
}

// NearEqualVector3 utility
TEST(MathUtil, NearEqualVector3Basic)
{
    XMFLOAT3 a(1.0f, 2.0f, 3.0f);
    XMFLOAT3 b(1.0f, 2.0f, 3.0f);
    XMFLOAT3 c(1.0f, 2.0f, 4.0f);

    EXPECT_TRUE(NearEqualVector3(a, b));
    EXPECT_FALSE(NearEqualVector3(a, c));
}

// Load/Store round-trip
TEST(MathUtil, LoadStoreRoundTrip)
{
    XMFLOAT3 v3(1.5f, -2.5f, 3.5f);
    XMFLOAT3 result3 = StoreVector3(LoadVector3(v3));
    EXPECT_TRUE(NearEqualVector3(v3, result3));

    XMFLOAT4 v4(1.0f, 2.0f, 3.0f, 4.0f);
    XMFLOAT4 result4 = StoreVector4(LoadVector4(v4));
    EXPECT_TRUE(NearEqual(v4.x, result4.x));
    EXPECT_TRUE(NearEqual(v4.y, result4.y));
    EXPECT_TRUE(NearEqual(v4.z, result4.z));
    EXPECT_TRUE(NearEqual(v4.w, result4.w));
}
