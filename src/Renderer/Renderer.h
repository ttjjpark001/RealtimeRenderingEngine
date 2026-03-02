#pragma once

#include "Core/Types.h"
#include "Renderer/FrustumCuller.h"
#include "Renderer/OcclusionCuller.h"
#include "Renderer/LODSelector.h"
#include "Renderer/LightCuller.h"
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
    RenderMode    m_renderMode   = RenderMode::FullPBRShadows;

    std::unordered_map<Mesh*, MeshBuffers> m_meshCache;

    // Cullers & LOD (owned by Renderer; rebuilt/reused each frame)
    FrustumCuller   m_frustumCuller;
    OcclusionCuller m_occlusionCuller;
    LODSelector     m_lodSelector;
    LightCuller     m_lightCuller;

    CullStats m_lastCullStats;
};

} // namespace RRE
