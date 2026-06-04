#include "RHI/D3D12/D3D12ComputePipeline.h"
#include <d3dcompiler.h>

namespace RRE
{

bool D3D12ComputePipeline::Initialize(ID3D12Device* device,
                                       const wchar_t* csoPath,
                                       ID3D12RootSignature* rootSignature)
{
    HRESULT hr = D3DReadFileToBlob(csoPath, &m_shader);
    if (FAILED(hr))
        return false;

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature;
    psoDesc.CS = { m_shader->GetBufferPointer(), m_shader->GetBufferSize() };

    hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_pso));
    return SUCCEEDED(hr);
}

void D3D12ComputePipeline::Shutdown()
{
    m_pso.Reset();
    m_shader.Reset();
}

} // namespace RRE
