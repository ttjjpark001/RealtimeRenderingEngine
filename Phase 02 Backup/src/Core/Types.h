#pragma once

#include <cstdint>
#include <DirectXMath.h>

namespace RRE
{
    using uint8  = std::uint8_t;
    using uint16 = std::uint16_t;
    using uint32 = std::uint32_t;
    using uint64 = std::uint64_t;
    using int32  = std::int32_t;
    using int64  = std::int64_t;

    // DirectXMath type aliases
    using Vector3   = DirectX::XMFLOAT3;
    using Vector4   = DirectX::XMFLOAT4;
    using Matrix4x4 = DirectX::XMFLOAT4X4;
    using Color     = DirectX::XMFLOAT4;

} // namespace RRE
