#pragma once

#include <DirectXMath.h>
#include <cmath>

namespace RRE
{
namespace Math
{

using namespace DirectX;

inline XMVECTOR LoadVector3(const XMFLOAT3& v)
{
    return XMLoadFloat3(&v);
}

inline XMFLOAT3 StoreVector3(FXMVECTOR v)
{
    XMFLOAT3 result;
    XMStoreFloat3(&result, v);
    return result;
}

inline XMVECTOR LoadVector4(const XMFLOAT4& v)
{
    return XMLoadFloat4(&v);
}

inline XMFLOAT4 StoreVector4(FXMVECTOR v)
{
    XMFLOAT4 result;
    XMStoreFloat4(&result, v);
    return result;
}

// TRS matrix: Scale first, then Rotation (ZXY order), then Translation
inline XMMATRIX CreateTRSMatrix(const XMFLOAT3& position, const XMFLOAT3& rotationEuler, const XMFLOAT3& scale)
{
    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX Rz = XMMatrixRotationZ(rotationEuler.z);
    XMMATRIX Rx = XMMatrixRotationX(rotationEuler.x);
    XMMATRIX Ry = XMMatrixRotationY(rotationEuler.y);
    XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);

    // S * Rz * Rx * Ry * T
    return S * Rz * Rx * Ry * T;
}

inline bool NearEqual(float a, float b, float epsilon = 1e-5f)
{
    return std::fabsf(a - b) <= epsilon;
}

inline bool NearEqualVector3(const XMFLOAT3& a, const XMFLOAT3& b, float epsilon = 1e-5f)
{
    XMVECTOR va = XMLoadFloat3(&a);
    XMVECTOR vb = XMLoadFloat3(&b);
    XMVECTOR eps = XMVectorReplicate(epsilon);
    return XMVector3NearEqual(va, vb, eps);
}

inline bool NearEqualMatrix(const XMFLOAT4X4& a, const XMFLOAT4X4& b, float epsilon = 1e-5f)
{
    const float* fa = reinterpret_cast<const float*>(&a);
    const float* fb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < 16; ++i)
    {
        if (!NearEqual(fa[i], fb[i], epsilon))
            return false;
    }
    return true;
}

} // namespace Math
} // namespace RRE
