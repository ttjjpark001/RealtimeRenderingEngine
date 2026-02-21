#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <cstddef>

namespace RRE
{

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT3 normal;
};

static_assert(offsetof(Vertex, position) == 0,  "position offset mismatch");
static_assert(offsetof(Vertex, color)    == 12, "color offset mismatch");
static_assert(offsetof(Vertex, normal)   == 28, "normal offset mismatch");
static_assert(sizeof(Vertex)             == 40, "Vertex size mismatch");

inline const D3D12_INPUT_ELEMENT_DESC VERTEX_INPUT_LAYOUT[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,     0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

inline constexpr UINT VERTEX_INPUT_LAYOUT_COUNT = _countof(VERTEX_INPUT_LAYOUT);

} // namespace RRE
