#include "RHI/D3D12/D3D12PipelineState.h"
#include "Renderer/Vertex.h"
#include <d3dcompiler.h>

namespace RRE
{

namespace
{

// Embedded HLSL shader source for runtime compilation
const char* SHADER_SOURCE = R"(
cbuffer ObjectConstants : register(b0)
{
    float4x4 World;
    float4x4 ViewProjection;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR;
    float3 normal   : NORMAL;
};

struct PSInput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR;
    float3 normal    : NORMAL;
    float3 worldPos  : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.worldPos = worldPos.xyz;
    output.position = mul(worldPos, ViewProjection);
    output.normal = normalize(mul(input.normal, (float3x3)World));
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
)";

} // anonymous namespace

bool D3D12PipelineState::Initialize(ID3D12Device* device)
{
    if (!CreateRootSignature(device))
        return false;
    if (!CompileShaders())
        return false;
    if (!CreatePipelineState(device))
        return false;
    return true;
}

void D3D12PipelineState::Shutdown()
{
    m_pipelineState.Reset();
    m_rootSignature.Reset();
    m_vertexShader.Reset();
    m_pixelShader.Reset();
}

bool D3D12PipelineState::CreateRootSignature(ID3D12Device* device)
{
    // Single root constant buffer view (CBV) at register b0
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParam.Constants.ShaderRegister = 0;
    rootParam.Constants.RegisterSpace = 0;
    // World (16 floats) + ViewProjection (16 floats) = 32 floats
    rootParam.Constants.Num32BitValues = 32;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 1;
    rsDesc.pParameters = &rootParam;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> serialized;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized, &error);
    if (FAILED(hr))
        return false;

    hr = device->CreateRootSignature(0, serialized->GetBufferPointer(),
        serialized->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
    return SUCCEEDED(hr);
}

bool D3D12PipelineState::CompileShaders()
{
    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3DCompile(SHADER_SOURCE, strlen(SHADER_SOURCE), "BasicColor.hlsl",
        nullptr, nullptr, "VSMain", "vs_5_1", compileFlags, 0,
        &m_vertexShader, &error);
    if (FAILED(hr))
        return false;

    hr = D3DCompile(SHADER_SOURCE, strlen(SHADER_SOURCE), "BasicColor.hlsl",
        nullptr, nullptr, "PSMain", "ps_5_1", compileFlags, 0,
        &m_pixelShader, &error);
    if (FAILED(hr))
        return false;

    return true;
}

bool D3D12PipelineState::CreatePipelineState(ID3D12Device* device)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();

    psoDesc.VS.pShaderBytecode = m_vertexShader->GetBufferPointer();
    psoDesc.VS.BytecodeLength = m_vertexShader->GetBufferSize();
    psoDesc.PS.pShaderBytecode = m_pixelShader->GetBufferPointer();
    psoDesc.PS.BytecodeLength = m_pixelShader->GetBufferSize();

    psoDesc.InputLayout.pInputElementDescs = VERTEX_INPUT_LAYOUT;
    psoDesc.InputLayout.NumElements = VERTEX_INPUT_LAYOUT_COUNT;

    // Rasterizer state
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    // Blend state (opaque)
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // Depth stencil
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
    return SUCCEEDED(hr);
}

} // namespace RRE
