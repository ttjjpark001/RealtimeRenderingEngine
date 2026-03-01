#pragma once

#include "Core/Types.h"
#include <memory>
#include <unordered_map>

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

    // Render entire scene graph
    void RenderScene(SceneGraph& graph, Camera& camera,
        float aspectRatio, LightManager* lightManager = nullptr);

    void ClearMeshCache();

private:
    struct MeshBuffers
    {
        std::unique_ptr<IRHIBuffer> vb;
        std::unique_ptr<IRHIBuffer> ib;
        uint32 indexCount = 0;
    };

    D3D12Context* m_context = nullptr;
    ID3D12Device* m_d3dDevice = nullptr;
    TextureCache* m_textureCache = nullptr;
    RenderMode m_renderMode = RenderMode::FullPBRShadows;
    std::unordered_map<Mesh*, MeshBuffers> m_meshCache;
};

} // namespace RRE
