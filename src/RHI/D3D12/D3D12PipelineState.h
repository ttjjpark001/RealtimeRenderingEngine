#pragma once

#include <d3d12.h>
#include <wrl/client.h>

namespace RRE
{

class D3D12PipelineState
{
public:
    D3D12PipelineState() = default;
    ~D3D12PipelineState() = default;

    bool Initialize(ID3D12Device* device);
    void Shutdown();

    ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }
    ID3D12PipelineState* GetPSO() const { return m_pipelineState.Get(); }

private:
    bool CreateRootSignature(ID3D12Device* device);
    bool CompileShaders();
    bool CreatePipelineState(ID3D12Device* device);

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
    Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader;
};

} // namespace RRE
