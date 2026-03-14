#pragma once

// Windows/DirectX headers must precede project headers here because
// RHIContext.h declares DrawTextW, which relies on HWND and other Win32
// types that are only available after <windows.h> is pulled in via d3d12.h.
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
#include "RHI/D3D12/D3D12CBPool.h"

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
static_assert(sizeof(PerObjectConstants) <= 256, "PerObjectConstants exceeds 256-byte CB slot");

// PBR per-object constants (b0) — viewProj and camera; world comes from per-instance buffer
struct PerObjectPBR
{
    DirectX::XMFLOAT4X4 viewProj;       // 64
    DirectX::XMFLOAT3 cameraPosition;   // 12
    float _pad0;                         // 4
};  // Total: 80 bytes → 256 aligned
static_assert(sizeof(PerObjectPBR) <= 256, "PerObjectPBR exceeds 256-byte CB slot");

// Wireframe pass constants (b0) — matches Wireframe.hlsl cbuffer exactly (world + viewProj + camPos)
struct WireframeConstants
{
    DirectX::XMFLOAT4X4 world;          // 64
    DirectX::XMFLOAT4X4 viewProj;       // 64
    DirectX::XMFLOAT3 cameraPosition;   // 12
    float _pad0;                         // 4
};  // Total: 144 bytes → 256 aligned
static_assert(sizeof(WireframeConstants) <= 256, "WireframeConstants exceeds 256-byte CB slot");

// Light data for PBR (matches HLSL LightData struct exactly)
struct GPULightData
{
    DirectX::XMFLOAT3 position;  // 12
    float intensity;              // 4
    DirectX::XMFLOAT3 color;    // 12
    float _pad0;                  // 4
    float Kc;                     // 4
    float Kl;                     // 4
    float Kq;                     // 4
    uint32 type;                  // 4  (0=Directional, 1=Point, 2=Spot)
    DirectX::XMFLOAT3 direction; // 12
    float innerConeAngle;         // 4
    float outerConeAngle;         // 4
    int32 shadowMapIndex;         // 4  (-1 = no shadow, 0~7 = shadow map index)
    float _pad1[2];               // 8
};  // 80 bytes per light

// Lights constant buffer (b1)
static constexpr uint32 MAX_PBR_LIGHTS = 16;
struct LightConstants
{
    GPULightData lights[MAX_PBR_LIGHTS]; // 80 * 16 = 1280
    uint32 numActiveLights;              // 4
    float _pad[3];                        // 12
};  // Total: 1296 bytes → 1536 aligned (256 * 6)
static_assert(sizeof(LightConstants) <= 1536, "LightConstants exceeds 1536-byte CB slot");

// Per-material constants (b2)
struct PerMaterialConstants
{
    DirectX::XMFLOAT4 baseColorFactor;   // 16
    float metallicFactor;                 // 4
    float roughnessFactor;                // 4
    float alphaCutoff;                    // 4
    uint32 hasAlbedoMap;                  // 4
    uint32 hasNormalMap;                  // 4
    uint32 hasMetallicRoughnessMap;       // 4
    uint32 hasEmissiveMap;                // 4
    uint32 hasOcclusionMap;               // 4
    DirectX::XMFLOAT3 emissiveFactor;    // 12
    uint32 alphaMode;                     // 4  (0=Opaque, 1=Mask, 2=Blend)
    uint32 useMips;                       // 4  (1=use mip chain, 0=force mip 0)
    uint32 _padMat[3];                    // 12 (padding to next multiple of 16)
};  // Total: 80 bytes → 256 aligned
static_assert(sizeof(PerMaterialConstants) <= 256, "PerMaterialConstants exceeds 256-byte CB slot");

// Shadow mapping constants
static constexpr uint32 MAX_SHADOW_MAPS = 8;

// Shadow constant buffer (b3)
struct ShadowConstants
{
    DirectX::XMFLOAT4X4 lightViewProj[MAX_SHADOW_MAPS]; // 64 * 8 = 512
    uint32 shadowMapCount;                                // 4
    float  shadowTexelSize;                               // 4  (= 1.0f / shadowMapSize)
    float  shadowNormalBiasWorld;                         // 4  (= orthoSize / mapSize * 2)
    float _pad;                                           // 4
};  // Total: 528 bytes → 768 aligned (256 * 3)
static_assert(sizeof(ShadowConstants) <= 768, "ShadowConstants exceeds 768-byte CB slot");

// Shadow depth pass per-batch CB (b0) — world comes from per-instance buffer
struct ShadowPassConstants
{
    DirectX::XMFLOAT4X4 lightViewProj;   // 64
    // world is no longer stored here; supplied via per-instance vertex stream
};  // Total: 64 bytes → 256 aligned
static_assert(sizeof(ShadowPassConstants) <= 256, "ShadowPassConstants exceeds 256-byte CB slot");

class Material;
class TextureCache;
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

    // Set PBR light data for current frame
    void SetPBRLightData(const LightConstants& lights) { m_pbrLightConstants = lights; }

    // Set shadow data for current frame
    void SetShadowData(const ShadowConstants& data) { m_shadowConstants = data; }

    // Set render mode for texture flag overrides (0=Wireframe..4=FullPBRShadows)
    void SetRenderModeInt(int mode) { m_renderModeInt = mode; }

    // Set mip-mapping enabled flag (controls useMips in PerMaterialConstants)
    void SetMipMappingEnabled(bool enabled) { m_mipMappingEnabled = enabled; }

    // PBR draw call (single instance — creates a 1-element instance buffer internally)
    void DrawPrimitivesPBR(IRHIBuffer* vb, IRHIBuffer* ib,
        const DirectX::XMFLOAT4X4& worldMatrix,
        Material* material, TextureCache* textureCache);

    // PBR draw call: instanced — all instances share the same Mesh + Material
    // worlds[] must contain instanceCount transposed world matrices
    void DrawPrimitivesPBRInstanced(IRHIBuffer* vb, IRHIBuffer* ib,
        const DirectX::XMFLOAT4X4* worlds, uint32 instanceCount,
        Material* material, TextureCache* textureCache);

    // Wireframe draw call: simple position-only rendering
    void DrawPrimitivesWireframe(IRHIBuffer* vb, IRHIBuffer* ib,
        const DirectX::XMFLOAT4X4& worldMatrix);

    // Shadow mapping methods
    void CreateShadowMaps();
    // Set desired shadow map resolution and recreate resources (call after WaitForGPU).
    // Size is clamped to [512, 4096] and rounded to a power of two.
    void SetShadowMapSize(uint32 size);
    uint32 GetShadowMapSize() const { return m_shadowMapSize; }
    void RecreateShadowMaps();
    void BeginShadowPass(uint32 shadowIndex);
    void EndShadowPass(uint32 shadowIndex);
    // Restore main back-buffer RTV + DSV + viewport after all shadow passes
    void RestoreMainRenderTarget();
    // Shadow depth — single instance (creates 1-element instance buffer internally)
    void DrawShadowDepth(IRHIBuffer* vb, IRHIBuffer* ib,
        const DirectX::XMFLOAT4X4& worldMatrix,
        const DirectX::XMFLOAT4X4& lightViewProj);

    // Shadow depth — instanced batch
    void DrawShadowDepthInstanced(IRHIBuffer* vb, IRHIBuffer* ib,
        const DirectX::XMFLOAT4X4* worlds, uint32 instanceCount,
        const DirectX::XMFLOAT4X4& lightViewProj);

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

    // Open/execute command list for resource uploads (outside render loop, uses Graphics Queue)
    void BeginUploadCommands();
    void EndUploadCommands();

    // Copy Queue API — for async resource uploads parallel to rendering
    void BeginCopyCommands();
    void EndCopyCommands();   // submits to Copy Queue and signals copy fence
    void WaitForCopyQueue();  // CPU-side wait for copy fence completion
    ID3D12GraphicsCommandList* GetCopyCommandList() const { return m_copyCommandList.Get(); }

    // Descriptor heap access for texture SRV creation
    D3D12DescriptorHeap& GetCBVSRVHeap() { return m_cbvSrvHeap; }

    void WaitForGPU();
    void CreateDepthBuffer(uint32 width, uint32 height);

private:
    void FlushTextCommands();

    ID3D12Device* m_device = nullptr;
    D3D12SwapChain* m_swapChain = nullptr;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // Fence for GPU synchronization (Graphics Queue)
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    uint64 m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;

    // Copy Queue — async resource upload parallel to rendering
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>        m_copyQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    m_copyCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_copyCommandList;
    Microsoft::WRL::ComPtr<ID3D12Fence>               m_copyFence;
    uint64 m_copyFenceValue = 0;
    HANDLE m_copyFenceEvent = nullptr;
    bool   m_copyCommandListOpen = false;

    // Per-frame instance data pool (Upload Heap ring buffer, no 256-byte alignment required)
    // Provides sub-allocations for per-instance world matrices.
    // Layout: [ frame-0 half | frame-1 half ] — each half is INSTANCE_POOL_HALF_SIZE bytes.
    static constexpr uint32 INSTANCE_POOL_HALF_SIZE = 2 * 1024 * 1024; // 2 MB per frame
    Microsoft::WRL::ComPtr<ID3D12Resource> m_instancePool;
    uint8*    m_instancePoolMapped  = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS m_instancePoolGpuBase = 0;
    uint32    m_instancePoolOffset  = 0;   // current offset within the active half
    uint32    m_instancePoolFrame   = 0;   // 0 or 1

    struct InstanceAlloc
    {
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddr;
        uint8*                    cpuPtr;
    };
    // Allocate 'instanceCount' InstanceData slots (16-byte aligned).
    // Returns {0, nullptr} if pool is exhausted (shouldn't happen with 2 MB budget).
    InstanceAlloc AllocateInstanceBuffer(uint32 instanceCount);

    // Pipeline state
    D3D12PipelineState m_pipelineState;
    bool m_hasPSO = false;

    // Depth buffer
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBuffer;
    D3D12DescriptorHeap m_dsvHeap;

    // Shadow map resources
    Microsoft::WRL::ComPtr<ID3D12Resource> m_shadowMaps[MAX_SHADOW_MAPS];
    D3D12DescriptorHeap m_shadowDsvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_shadowSrvCpu[MAX_SHADOW_MAPS] = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_shadowSrvGpu[MAX_SHADOW_MAPS] = {};
    bool m_shadowMapsCreated   = false;
    bool m_shadowSrvsAllocated = false;  // persistent SRV slots allocated once; reused on resize
    uint32 m_shadowMapSize     = 1024;  // runtime-configurable resolution

    // Constant buffer pool (replaces fixed 16-slot CB)
    D3D12CBPool m_cbPool;

    // Unified CBV+SRV descriptor heap (shader-visible)
    // Persistent region: texture SRVs
    // Transient region: per-frame CBVs
    static constexpr uint32 PERSISTENT_DESCRIPTORS = 2048;
    static constexpr uint32 TRANSIENT_DESCRIPTORS = 32768;
    D3D12DescriptorHeap m_cbvSrvHeap;
    UINT m_cbvDescriptorSize = 0;

    // Frame counter for double buffering
    uint32 m_frameCounter = 0;

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

    // PBR lighting data (set via SetPBRLightData)
    LightConstants m_pbrLightConstants = {};

    // Render mode (mirrors RenderMode enum: 0=Wireframe..4=FullPBRShadows)
    int m_renderModeInt = 4;

    // Mip-mapping toggle
    bool m_mipMappingEnabled = true;

    // Shadow data (set via SetShadowData)
    ShadowConstants m_shadowConstants = {};

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
