#pragma once

#include <d3d12.h>
#include <wrl/client.h>

namespace RRE
{

// Wraps a Compute Shader pipeline state object (CS PSO).
// The root signature is created externally (in D3D12Context) and passed in.
class D3D12ComputePipeline
{
public:
    D3D12ComputePipeline() = default;
    ~D3D12ComputePipeline() = default;

    // Load a precompiled .cso and create the compute PSO.
    // rootSignature must outlive this object.
    bool Initialize(ID3D12Device* device,
                    const wchar_t* csoPath,
                    ID3D12RootSignature* rootSignature);

    void Shutdown();

    ID3D12PipelineState* GetPSO() const { return m_pso.Get(); }
    bool IsValid() const { return m_pso != nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
    Microsoft::WRL::ComPtr<ID3DBlob>            m_shader;
};

} // namespace RRE
