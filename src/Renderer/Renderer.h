#pragma once

#include "Core/Types.h"
#include "Renderer/FrustumCuller.h"
#include "Renderer/OcclusionCuller.h"
#include "Renderer/LODSelector.h"
#include "Renderer/LightCuller.h"
#include "Renderer/InstanceBatcher.h"
#include <memory>
#include <unordered_map>
#include <vector>

struct ID3D12Device;

namespace RRE
{

enum class RenderMode
{
    Wireframe,
    Solid,
    BaseColorOnly,
    FullPBR,
    FullPBRShadows
};

// Per-frame culling/LOD statistics populated by RenderScene().
struct CullStats
{
    uint32 visibleNodes         = 0;  // nodes drawn after all culling passes
    uint32 frustumCulledNodes   = 0;  // nodes rejected by frustum cull
    uint32 occlusionCulledNodes = 0;  // nodes rejected by occlusion cull (P0: always 0)
    uint32 activeLights         = 0;  // lights submitted to GPU after light culling
    uint32 culledLights         = 0;  // Point/Spot lights rejected by light culler
    uint32 renderedPolygons     = 0;  // actual triangles submitted (after culling + LOD)
    // Instancing stats (Phase 29)
    uint32 opaqueBatches        = 0;  // unique (Mesh*, Material*) batches after front-to-back sort
    uint32 totalInstances       = 0;  // total opaque instances across all batches
};

class D3D12Context;
class IRHIBuffer;
class Mesh;
class SceneGraph;
class Camera;
class LightManager;
class TextureCache;

class Renderer
{
public:
    Renderer() = default;
    ~Renderer() = default;

    void SetContext(D3D12Context* context, ID3D12Device* device);
    void SetTextureCache(TextureCache* cache) { m_textureCache = cache; }

    void SetRenderMode(RenderMode mode) { m_renderMode = mode; }
    RenderMode GetRenderMode() const { return m_renderMode; }

    void SetLODEnabled(bool enabled)            { m_lodEnabled = enabled; }
    bool IsLODEnabled() const                   { return m_lodEnabled; }

    void SetMipMappingEnabled(bool enabled);
    bool IsMipMappingEnabled() const            { return m_mipMappingEnabled; }

    // VRAM pressure hint: when true, LOD distances are scaled up so objects switch
    // to lower-detail meshes sooner, reducing per-frame polygon throughput.
    void SetVRAMPressure(bool highPressure)     { m_vramPressure = highPressure; }
    bool IsVRAMPressure() const                 { return m_vramPressure; }

    // Scene diagonal length — used to scale shadow ortho projection and far plane.
    // Call after loading a scene whenever sceneDiagonal is known.
    void SetSceneDiagonal(float d)              { m_sceneDiagonal = (d > 0.f ? d : 1.f); }

    // Scene world-space center — used to position the directional shadow camera so
    // the entire scene falls within the shadow map depth range.
    void SetSceneCenter(const DirectX::XMFLOAT3& center) { m_sceneCenter = center; }

    void SetFrustumCullingEnabled(bool enabled) { m_frustumCullingEnabled = enabled; }
    bool IsFrustumCullingEnabled() const        { return m_frustumCullingEnabled; }

    void SetOcclusionCullingEnabled(bool enabled);
    bool IsOcclusionCullingEnabled() const      { return m_occlusionCullingEnabled; }

    void SetLightCullingEnabled(bool enabled)   { m_lightCullingEnabled = enabled; }
    bool IsLightCullingEnabled() const          { return m_lightCullingEnabled; }

    // CSM (Cascaded Shadow Maps) — Phase 33a
    void SetCSMEnabled(bool enabled)            { m_csmEnabled = enabled; }
    bool IsCSMEnabled() const                   { return m_csmEnabled; }
    void SetCSMDebugView(bool enabled)          { m_csmDebugView = enabled; }
    bool IsCSMDebugView() const                 { return m_csmDebugView; }

    void SetPCSSEnabled(bool enabled)           { m_pcssEnabled = enabled; }
    bool IsPCSSEnabled() const                  { return m_pcssEnabled; }

    static constexpr uint32 CSM_NUM_CASCADES = 3;

    // Upload mesh VB/IB to GPU (cached, idempotent)
    void UploadMesh(Mesh* mesh);

    // Render the scene graph with frustum culling, occlusion culling (stub), and LOD.
    void RenderScene(SceneGraph& graph, Camera& camera,
        float aspectRatio, LightManager* lightManager = nullptr);

    // Clear GPU mesh cache AND all LOD registrations (safe to call before scene reload).
    void ClearMeshCache();

    // Register all loaded meshes for async auto-LOD generation.
    // Switch distances scale with sceneDiagonal so larger scenes transition later.
    void RegisterMeshesForLOD(const std::vector<std::unique_ptr<Mesh>>& meshes,
                               float sceneDiagonal);

    // Culling / LOD statistics from the last RenderScene() call.
    CullStats GetLastCullStats() const { return m_lastCullStats; }

private:
    struct MeshBuffers
    {
        std::unique_ptr<IRHIBuffer> vb;
        std::unique_ptr<IRHIBuffer> ib;
        uint32 indexCount = 0;
    };

    D3D12Context* m_context      = nullptr;
    ID3D12Device* m_d3dDevice    = nullptr;
    TextureCache* m_textureCache = nullptr;
    RenderMode    m_renderMode             = RenderMode::FullPBRShadows;
    bool          m_lodEnabled             = false;
    bool          m_mipMappingEnabled      = true;
    bool          m_frustumCullingEnabled  = true;
    bool          m_occlusionCullingEnabled = false;
    bool          m_lightCullingEnabled    = true;
    bool          m_vramPressure           = false;
    bool          m_csmEnabled             = true;
    bool          m_csmDebugView           = false;
    bool          m_pcssEnabled            = false;
    float         m_sceneDiagonal          = 10.0f;
    DirectX::XMFLOAT3 m_sceneCenter        = { 0.f, 0.f, 0.f };

    std::unordered_map<Mesh*, MeshBuffers> m_meshCache;

    // Cullers & LOD (owned by Renderer; rebuilt/reused each frame)
    FrustumCuller   m_frustumCuller;
    OcclusionCuller m_occlusionCuller;
    LODSelector     m_lodSelector;
    LightCuller     m_lightCuller;

    // Instance batching: groups same-mesh+material nodes into single draw calls
    InstanceBatcher m_opaqueBatcher;
    InstanceBatcher m_alphaMaskBatcher;

    // Hi-Z Occlusion Culling (Phase 32) — node lists used across frames
    struct HiZNode { SceneNode* node; DirectX::BoundingBox aabb; };
    std::vector<HiZNode>    m_hizNodeList;    // frustum-visible nodes this frame → next frame's test
    std::vector<SceneNode*> m_hizNodeOrder;   // node order submitted in previous frame's test

    CullStats m_lastCullStats;
};

} // namespace RRE
