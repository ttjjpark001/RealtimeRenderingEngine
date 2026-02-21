#pragma once

#include <d3d12.h>
#include <d3d11on12.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include "Core/Types.h"
#include "RHI/RHIContext.h"
#include "RHI/D3D12/D3D12PipelineState.h"
#include "RHI/D3D12/D3D12DescriptorHeap.h"

namespace RRE
{

// Constant buffer data passed to the GPU per draw call
struct PerObjectConstants
{
    DirectX::XMFLOAT4X4 world;          // 64
    DirectX::XMFLOAT4X4 viewProj;       // 64
    DirectX::XMFLOAT3 lightPosition;    // 12
    float _pad1;                         // 4
    DirectX::XMFLOAT3 lightColor;       // 12
    float _pad2;                         // 4
    DirectX::XMFLOAT3 cameraPosition;   // 12
    float _pad3;                         // 4
    DirectX::XMFLOAT3 ambientColor;     // 12
    float _pad4;                         // 4
    float Kc;                            // 4
    float Kl;                            // 4
    float Kq;                            // 4
    float unlit;                         // 4
    DirectX::XMFLOAT3 colorOverride;    // 12
    float _pad6;                         // 4
};  // Total: 224 bytes → 256 aligned

class D3D12SwapChain;

struct TextCommand
{
    int x;
    int y;
    std::string text;
    DirectX::XMFLOAT4 color;
};

class D3D12Context : public IRHIContext
{
public:
    D3D12Context() = default;
    ~D3D12Context() override = default;

    bool Initialize(ID3D12Device* device);
    void Shutdown();

    void SetSwapChain(D3D12SwapChain* swapChain) { m_swapChain = swapChain; }

    // D2D text rendering
    bool InitializeD2D(ID3D12Device* device, ID3D12CommandQueue* commandQueue,
        D3D12SwapChain* swapChain);
    void CreateD2DRenderTargets(D3D12SwapChain* swapChain);
    void ReleaseD2DRenderTargets();
    void ShutdownD2D();

    // Set View-Projection matrix for current frame
    void SetViewProjection(const DirectX::XMFLOAT4X4& viewProj) { m_viewProjection = viewProj; }

    // Set lighting data for current frame
    void SetLightData(const DirectX::XMFLOAT3& lightPos, const DirectX::XMFLOAT3& lightColor,
        const DirectX::XMFLOAT3& cameraPos, const DirectX::XMFLOAT3& ambient,
        float Kc, float Kl, float Kq)
    {
        m_lightPosition = lightPos;
        m_lightColor = lightColor;
        m_cameraPosition = cameraPos;
        m_ambientColor = ambient;
        m_Kc = Kc; m_Kl = Kl; m_Kq = Kq;
    }

    // Set unlit mode for next draw call (solid color, no lighting)
    void SetUnlitMode(bool unlit, const DirectX::XMFLOAT3& color)
    {
        m_unlit = unlit ? 1.0f : 0.0f;
        m_colorOverride = color;
    }

    // IRHIContext interface
    void BeginFrame() override;
    void EndFrame() override;
    void Clear(const DirectX::XMFLOAT4& color) override;
    void DrawPrimitives(IRHIBuffer* vb, IRHIBuffer* ib,
        const DirectX::XMFLOAT4X4& worldMatrix) override;
    void DrawText(int x, int y, const char* text,
        const DirectX::XMFLOAT4& color) override;

    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }

    void WaitForGPU();
    void CreateDepthBuffer(uint32 width, uint32 height);

private:
    void MoveToNextFrame();
    bool CreateConstantBuffer();
    void FlushTextCommands();

    ID3D12Device* m_device = nullptr;
    D3D12SwapChain* m_swapChain = nullptr;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // Fence for GPU synchronization
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    uint64 m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;

    // Pipeline state
    D3D12PipelineState m_pipelineState;
    bool m_hasPSO = false;

    // Depth buffer
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBuffer;
    D3D12DescriptorHeap m_dsvHeap;

    // Constant buffer (CBV) — supports multiple draw calls per frame
    static constexpr uint32 MAX_DRAW_CALLS = 16;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    D3D12DescriptorHeap m_cbvHeap;
    uint8* m_cbData = nullptr;
    uint32 m_drawCallIndex = 0;
    UINT m_cbAlignedSize = 0;

    // Current frame's view-projection matrix
    DirectX::XMFLOAT4X4 m_viewProjection;

    // Current frame's lighting data
    DirectX::XMFLOAT3 m_lightPosition = { 2.0f, 3.0f, -2.0f };
    DirectX::XMFLOAT3 m_lightColor = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 m_cameraPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_ambientColor = { 0.15f, 0.15f, 0.15f };
    float m_Kc = 1.0f, m_Kl = 0.09f, m_Kq = 0.032f;

    // Unlit mode (for light indicator)
    float m_unlit = 0.0f;
    DirectX::XMFLOAT3 m_colorOverride = { 1.0f, 1.0f, 1.0f };

    // D3D11On12 / D2D / DirectWrite
    Microsoft::WRL::ComPtr<ID3D11On12Device> m_d3d11On12Device;
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11DeviceContext;
    Microsoft::WRL::ComPtr<ID2D1Factory1> m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device> m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_d2dDeviceContext;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormat;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_textBrush;

    // Per-back-buffer wrapped resources
    static constexpr uint32 MAX_BACK_BUFFERS = 2;
    Microsoft::WRL::ComPtr<ID3D11Resource> m_wrappedBackBuffers[MAX_BACK_BUFFERS];
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_d2dRenderTargets[MAX_BACK_BUFFERS];

    // Queued text commands
    std::vector<TextCommand> m_textCommands;
    bool m_d2dInitialized = false;
};

} // namespace RRE
