#include "RHI/D3D12/D3D12PipelineState.h"
#include "Renderer/Vertex.h"
#include <d3dcompiler.h>
#include <string>

namespace RRE
{

bool D3D12PipelineState::Initialize(ID3D12Device* device)
{
    if (!CreateRootSignature(device))
        return false;
    if (!LoadShaders())
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
    // Root Parameter 0: CBV descriptor table at register b0 (PerObject constants)
    D3D12_DESCRIPTOR_RANGE cbvRange = {};
    cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvRange.NumDescriptors = 1;
    cbvRange.BaseShaderRegister = 0;
    cbvRange.RegisterSpace = 0;
    cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Root Parameter 1: SRV descriptor table at register t0~t4 (Material textures)
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 5;
    srvRange.BaseShaderRegister = 0;  // t0~t4
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[2] = {};

    // Param 0: CBV table (b0)
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[0].DescriptorTable.pDescriptorRanges = &cbvRange;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Param 1: SRV table (t0~t4)
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &srvRange;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static sampler s0: Anisotropic Wrap
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_ANISOTROPIC;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MaxAnisotropy = 16;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;  // s0
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 2;
    rsDesc.pParameters = rootParams;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers = &sampler;
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

bool D3D12PipelineState::LoadShaders()
{
    // Get executable directory to find precompiled .cso files
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\") + 1);

    std::wstring vsPath = exeDir + L"Shaders\\VertexShader.cso";
    std::wstring psPath = exeDir + L"Shaders\\PixelShader.cso";

    HRESULT hr = D3DReadFileToBlob(vsPath.c_str(), &m_vertexShader);
    if (FAILED(hr))
        return false;

    hr = D3DReadFileToBlob(psPath.c_str(), &m_pixelShader);
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
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
    return SUCCEEDED(hr);
}

} // namespace RRE
