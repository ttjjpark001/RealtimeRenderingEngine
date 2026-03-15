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
    DirectX::XMFLOAT2 texCoord;
    DirectX::XMFLOAT4 tangent;
};

static_assert(offsetof(Vertex, position) == 0,  "position offset mismatch");
static_assert(offsetof(Vertex, color)    == 12, "color offset mismatch");
static_assert(offsetof(Vertex, normal)   == 28, "normal offset mismatch");
static_assert(offsetof(Vertex, texCoord) == 40, "texCoord offset mismatch");
static_assert(offsetof(Vertex, tangent)  == 48, "tangent offset mismatch");
static_assert(sizeof(Vertex)             == 64, "Vertex size mismatch");

// Per-instance data for instanced rendering (world matrix, already transposed for GPU)
struct InstanceData
{
    DirectX::XMFLOAT4X4 world; // 64 bytes — transposed world matrix
};
static_assert(sizeof(InstanceData) == 64, "InstanceData size mismatch");

// Per-vertex input layout (slot 0) — used by BasicColor and Wireframe PSOs
inline const D3D12_INPUT_ELEMENT_DESC VERTEX_INPUT_LAYOUT[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,     0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};
inline constexpr UINT VERTEX_INPUT_LAYOUT_COUNT = _countof(VERTEX_INPUT_LAYOUT);

// Per-vertex (slot 0) + per-instance world matrix rows (slot 1) — used by PBR and Shadow PSOs
inline const D3D12_INPUT_ELEMENT_DESC INSTANCED_VERTEX_INPUT_LAYOUT[] =
{
    // Slot 0: per-vertex data
    { "POSITION",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
    { "COLOR",         0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
    { "NORMAL",        0, DXGI_FORMAT_R32G32B32_FLOAT,     0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
    { "TEXCOORD",      0, DXGI_FORMAT_R32G32_FLOAT,        0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
    { "TANGENT",       0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
    // Slot 1: per-instance world matrix (4 rows of float4)
    { "INSTANCE_WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    { "INSTANCE_WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    { "INSTANCE_WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    { "INSTANCE_WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
};
inline constexpr UINT INSTANCED_VERTEX_INPUT_LAYOUT_COUNT = _countof(INSTANCED_VERTEX_INPUT_LAYOUT);

} // namespace RRE
