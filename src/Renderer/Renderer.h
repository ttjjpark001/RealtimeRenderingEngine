#pragma once

#include "Core/Types.h"
#include <memory>
#include <unordered_map>

struct ID3D12Device;

namespace RRE
{

class D3D12Context;
class IRHIBuffer;
class Mesh;
class SceneGraph;
class Camera;
class PointLight;
class LightManager;
class TextureCache;

class Renderer
{
public:
    Renderer() = default;
    ~Renderer() = default;

    void SetContext(D3D12Context* context, ID3D12Device* device);
    void SetTextureCache(TextureCache* cache) { m_textureCache = cache; }

    // Upload mesh VB/IB to GPU (cached, idempotent)
    void UploadMesh(Mesh* mesh);

    // Render entire scene graph
    void RenderScene(SceneGraph& graph, Camera& camera, PointLight* light,
        float aspectRatio, LightManager* lightManager = nullptr);

    // Render light indicator sphere (unlit)
    void RenderLightIndicator(PointLight* light, bool show,
        IRHIBuffer* sphereVB, IRHIBuffer* sphereIB);

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
    std::unordered_map<Mesh*, MeshBuffers> m_meshCache;
};

} // namespace RRE
