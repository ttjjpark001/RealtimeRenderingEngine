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

    // PBR PSO is optional — if shaders not found, skip silently
    if (LoadPBRShaders())
        CreatePBRPipelineState(device);

    // Shadow Depth PSO is optional
    if (LoadShadowDepthShaders())
        CreateShadowDepthPipelineState(device);

    // Alpha Blend PSO (requires PBR shaders)
    if (m_pbrVertexShader && m_pbrPixelShader)
        CreatePBRAlphaBlendPipelineState(device);

    // Wireframe PSO is optional
    if (LoadWireframeShaders())
        CreateWireframePipelineState(device);

    return true;
}

void D3D12PipelineState::Shutdown()
{
    m_wireframePipelineState.Reset();
    m_wireframeVertexShader.Reset();
    m_wireframePixelShader.Reset();
    m_pbrAlphaBlendPipelineState.Reset();
    m_shadowDepthPipelineState.Reset();
    m_shadowDepthVertexShader.Reset();
    m_pbrPipelineState.Reset();
    m_pbrVertexShader.Reset();
    m_pbrPixelShader.Reset();
    m_pipelineState.Reset();
    m_rootSignature.Reset();
    m_vertexShader.Reset();
    m_pixelShader.Reset();
}

bool D3D12PipelineState::CreateRootSignature(ID3D12Device* device)
{
    // Root Parameter 0: CBV descriptor table at register b0 (PerObject constants)
    D3D12_DESCRIPTOR_RANGE cbvRange0 = {};
    cbvRange0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvRange0.NumDescriptors = 1;
    cbvRange0.BaseShaderRegister = 0;
    cbvRange0.RegisterSpace = 0;
    cbvRange0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Root Parameter 1: SRV descriptor table at register t0~t4 (Material textures)
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 5;
    srvRange.BaseShaderRegister = 0;  // t0~t4
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Root Parameter 2: CBV descriptor table at register b1 (LightsCB)
    D3D12_DESCRIPTOR_RANGE cbvRange1 = {};
    cbvRange1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvRange1.NumDescriptors = 1;
    cbvRange1.BaseShaderRegister = 1;
    cbvRange1.RegisterSpace = 0;
    cbvRange1.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Root Parameter 3: CBV descriptor table at register b2 (PerMaterialCB)
    D3D12_DESCRIPTOR_RANGE cbvRange2 = {};
    cbvRange2.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvRange2.NumDescriptors = 1;
    cbvRange2.BaseShaderRegister = 2;
    cbvRange2.RegisterSpace = 0;
    cbvRange2.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Root Parameter 4: SRV descriptor table at register t5~t12 (Shadow Maps)
    D3D12_DESCRIPTOR_RANGE shadowSrvRange = {};
    shadowSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    shadowSrvRange.NumDescriptors = 8;
    shadowSrvRange.BaseShaderRegister = 5;  // t5~t12
    shadowSrvRange.RegisterSpace = 0;
    shadowSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Root Parameter 5: CBV descriptor table at register b3 (ShadowCB)
    D3D12_DESCRIPTOR_RANGE cbvRange3 = {};
    cbvRange3.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvRange3.NumDescriptors = 1;
    cbvRange3.BaseShaderRegister = 3;
    cbvRange3.RegisterSpace = 0;
    cbvRange3.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[6] = {};

    // Param 0: CBV table (b0)
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[0].DescriptorTable.pDescriptorRanges = &cbvRange0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Param 1: SRV table (t0~t4)
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &srvRange;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Param 2: CBV table (b1, LightsCB)
    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[2].DescriptorTable.pDescriptorRanges = &cbvRange1;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Param 3: CBV table (b2, PerMaterialCB)
    rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[3].DescriptorTable.pDescriptorRanges = &cbvRange2;
    rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Param 4: SRV table (t5~t12, Shadow Maps)
    rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[4].DescriptorTable.pDescriptorRanges = &shadowSrvRange;
    rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Param 5: CBV table (b3, ShadowCB)
    rootParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[5].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[5].DescriptorTable.pDescriptorRanges = &cbvRange3;
    rootParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static samplers
    D3D12_STATIC_SAMPLER_DESC samplers[2] = {};

    // s0: Anisotropic Wrap (material textures)
    samplers[0].Filter = D3D12_FILTER_ANISOTROPIC;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].MaxAnisotropy = 16;
    samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    samplers[0].MinLOD = 0.0f;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[0].ShaderRegister = 0;  // s0
    samplers[0].RegisterSpace = 0;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // s1: Comparison sampler for shadow mapping (PCF)
    samplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplers[1].MaxAnisotropy = 1;
    samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    samplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplers[1].MinLOD = 0.0f;
    samplers[1].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[1].ShaderRegister = 1;  // s1
    samplers[1].RegisterSpace = 0;
    samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 6;
    rsDesc.pParameters = rootParams;
    rsDesc.NumStaticSamplers = 2;
    rsDesc.pStaticSamplers = samplers;
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

bool D3D12PipelineState::LoadPBRShaders()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\") + 1);

    std::wstring vsPath = exeDir + L"Shaders\\PBR_VS.cso";
    std::wstring psPath = exeDir + L"Shaders\\PBR_PS.cso";

    HRESULT hr = D3DReadFileToBlob(vsPath.c_str(), &m_pbrVertexShader);
    if (FAILED(hr))
        return false;

    hr = D3DReadFileToBlob(psPath.c_str(), &m_pbrPixelShader);
    if (FAILED(hr))
        return false;

    return true;
}

bool D3D12PipelineState::CreatePBRPipelineState(ID3D12Device* device)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();

    psoDesc.VS.pShaderBytecode = m_pbrVertexShader->GetBufferPointer();
    psoDesc.VS.BytecodeLength = m_pbrVertexShader->GetBufferSize();
    psoDesc.PS.pShaderBytecode = m_pbrPixelShader->GetBufferPointer();
    psoDesc.PS.BytecodeLength = m_pbrPixelShader->GetBufferSize();

    // PBR uses instanced layout: per-vertex (slot 0) + per-instance world (slot 1)
    psoDesc.InputLayout.pInputElementDescs = INSTANCED_VERTEX_INPUT_LAYOUT;
    psoDesc.InputLayout.NumElements = INSTANCED_VERTEX_INPUT_LAYOUT_COUNT;

    // Rasterizer state — two-sided (NONE) so double-sided / winding-mismatch meshes render correctly;
    // back-face lighting handled in PS via SV_IsFrontFace
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
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

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pbrPipelineState));
    return SUCCEEDED(hr);
}

bool D3D12PipelineState::LoadShadowDepthShaders()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\") + 1);

    std::wstring vsPath = exeDir + L"Shaders\\ShadowDepth_VS.cso";

    HRESULT hr = D3DReadFileToBlob(vsPath.c_str(), &m_shadowDepthVertexShader);
    if (FAILED(hr))
        return false;

    return true;
}

bool D3D12PipelineState::CreateShadowDepthPipelineState(ID3D12Device* device)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();

    psoDesc.VS.pShaderBytecode = m_shadowDepthVertexShader->GetBufferPointer();
    psoDesc.VS.BytecodeLength = m_shadowDepthVertexShader->GetBufferSize();
    // No PS — depth-only rendering

    // Shadow uses instanced layout (world matrix comes from per-instance slot 1)
    psoDesc.InputLayout.pInputElementDescs = INSTANCED_VERTEX_INPUT_LAYOUT;
    psoDesc.InputLayout.NumElements = INSTANCED_VERTEX_INPUT_LAYOUT_COUNT;

    // Rasterizer state with depth bias for shadow acne prevention
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.DepthBias = 10000;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.RasterizerState.SlopeScaledDepthBias = 2.0f;

    // No blend state needed (no render targets)

    // Depth stencil
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 0;  // Depth-only, no color output
    psoDesc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_shadowDepthPipelineState));
    return SUCCEEDED(hr);
}

bool D3D12PipelineState::CreatePBRAlphaBlendPipelineState(ID3D12Device* device)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();

    psoDesc.VS.pShaderBytecode = m_pbrVertexShader->GetBufferPointer();
    psoDesc.VS.BytecodeLength = m_pbrVertexShader->GetBufferSize();
    psoDesc.PS.pShaderBytecode = m_pbrPixelShader->GetBufferPointer();
    psoDesc.PS.BytecodeLength = m_pbrPixelShader->GetBufferSize();

    // Alpha blend PSO also uses instanced layout
    psoDesc.InputLayout.pInputElementDescs = INSTANCED_VERTEX_INPUT_LAYOUT;
    psoDesc.InputLayout.NumElements = INSTANCED_VERTEX_INPUT_LAYOUT_COUNT;

    // Rasterizer state — two-sided (NONE); back-face lighting handled via SV_IsFrontFace in PS
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    // Blend state: alpha blending enabled
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // Depth stencil: depth read only (no write) for transparency
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pbrAlphaBlendPipelineState));
    return SUCCEEDED(hr);
}

bool D3D12PipelineState::LoadWireframeShaders()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\") + 1);

    std::wstring vsPath = exeDir + L"Shaders\\Wireframe_VS.cso";
    std::wstring psPath = exeDir + L"Shaders\\Wireframe_PS.cso";

    HRESULT hr = D3DReadFileToBlob(vsPath.c_str(), &m_wireframeVertexShader);
    if (FAILED(hr))
        return false;

    hr = D3DReadFileToBlob(psPath.c_str(), &m_wireframePixelShader);
    if (FAILED(hr))
        return false;

    return true;
}

bool D3D12PipelineState::CreateWireframePipelineState(ID3D12Device* device)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();

    psoDesc.VS.pShaderBytecode = m_wireframeVertexShader->GetBufferPointer();
    psoDesc.VS.BytecodeLength = m_wireframeVertexShader->GetBufferSize();
    psoDesc.PS.pShaderBytecode = m_wireframePixelShader->GetBufferPointer();
    psoDesc.PS.BytecodeLength = m_wireframePixelShader->GetBufferSize();

    psoDesc.InputLayout.pInputElementDescs = VERTEX_INPUT_LAYOUT;
    psoDesc.InputLayout.NumElements = VERTEX_INPUT_LAYOUT_COUNT;

    // Rasterizer state: wireframe fill mode
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
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

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_wireframePipelineState));
    return SUCCEEDED(hr);
}

} // namespace RRE
