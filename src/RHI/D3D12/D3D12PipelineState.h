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
    ID3D12PipelineState* GetPBRPSO() const { return m_pbrPipelineState.Get(); }
    bool HasPBRPSO() const { return m_pbrPipelineState != nullptr; }
    ID3D12PipelineState* GetShadowDepthPSO() const { return m_shadowDepthPipelineState.Get(); }
    bool HasShadowDepthPSO() const { return m_shadowDepthPipelineState != nullptr; }
    ID3D12PipelineState* GetPBRAlphaBlendPSO() const { return m_pbrAlphaBlendPipelineState.Get(); }
    bool HasPBRAlphaBlendPSO() const { return m_pbrAlphaBlendPipelineState != nullptr; }
    ID3D12PipelineState* GetWireframePSO() const { return m_wireframePipelineState.Get(); }
    bool HasWireframePSO() const { return m_wireframePipelineState != nullptr; }

private:
    bool CreateRootSignature(ID3D12Device* device);
    bool LoadShaders();
    bool CreatePipelineState(ID3D12Device* device);
    bool LoadPBRShaders();
    bool CreatePBRPipelineState(ID3D12Device* device);
    bool LoadShadowDepthShaders();
    bool CreateShadowDepthPipelineState(ID3D12Device* device);
    bool CreatePBRAlphaBlendPipelineState(ID3D12Device* device);
    bool LoadWireframeShaders();
    bool CreateWireframePipelineState(ID3D12Device* device);

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

    // BasicColor PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
    Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader;

    // PBR PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pbrPipelineState;
    Microsoft::WRL::ComPtr<ID3DBlob> m_pbrVertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> m_pbrPixelShader;

    // Shadow Depth PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_shadowDepthPipelineState;
    Microsoft::WRL::ComPtr<ID3DBlob> m_shadowDepthVertexShader;

    // PBR Alpha Blend PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pbrAlphaBlendPipelineState;

    // Wireframe PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_wireframePipelineState;
    Microsoft::WRL::ComPtr<ID3DBlob> m_wireframeVertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> m_wireframePixelShader;
};

} // namespace RRE
