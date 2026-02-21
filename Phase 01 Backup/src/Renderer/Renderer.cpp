#include "Renderer/Renderer.h"
#include "Renderer/Mesh.h"
#include "Renderer/Vertex.h"
#include "RHI/D3D12/D3D12Context.h"
#include "RHI/D3D12/D3D12Buffer.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneNode.h"
#include "Scene/Camera.h"
#include "Lighting/PointLight.h"
#include <DirectXMath.h>
#include <d3d12.h>

using namespace DirectX;

namespace RRE
{

void Renderer::SetContext(D3D12Context* context, ID3D12Device* device)
{
    m_context = context;
    m_d3dDevice = device;
}

void Renderer::UploadMesh(Mesh* mesh)
{
    if (!mesh || !m_d3dDevice || m_meshCache.count(mesh))
        return;

    MeshBuffers buffers;

    auto vb = std::make_unique<D3D12Buffer>();
    uint32 vbSize = static_cast<uint32>(mesh->vertices.size() * sizeof(Vertex));
    vb->Initialize(m_d3dDevice, mesh->vertices.data(), vbSize, sizeof(Vertex));
    buffers.vb = std::move(vb);

    auto ib = std::make_unique<D3D12Buffer>();
    uint32 ibSize = static_cast<uint32>(mesh->indices.size() * sizeof(uint32));
    ib->Initialize(m_d3dDevice, mesh->indices.data(), ibSize, sizeof(uint32));
    buffers.ib = std::move(ib);

    buffers.indexCount = static_cast<uint32>(mesh->indices.size());

    m_meshCache[mesh] = std::move(buffers);
}

void Renderer::ClearMeshCache()
{
    m_meshCache.clear();
}

void Renderer::RenderScene(SceneGraph& graph, Camera& camera, PointLight* light,
    float aspectRatio)
{
    if (!m_context)
        return;

    // Set view-projection from camera
    XMMATRIX view = camera.GetViewMatrix();
    XMMATRIX projection = camera.GetProjectionMatrix(aspectRatio);
    XMMATRIX viewProj = XMMatrixTranspose(view * projection);
    XMFLOAT4X4 viewProjFloat;
    XMStoreFloat4x4(&viewProjFloat, viewProj);
    m_context->SetViewProjection(viewProjFloat);

    // Set lighting data
    if (light)
    {
        XMFLOAT3 camPos = camera.GetPosition();
        XMFLOAT3 ambient = { 0.15f, 0.15f, 0.15f };
        m_context->SetLightData(
            light->GetPosition(), light->GetColor(),
            camPos, ambient,
            light->GetConstantAttenuation(),
            light->GetLinearAttenuation(),
            light->GetQuadraticAttenuation());
    }

    // Traverse scene graph and draw each node with a mesh
    graph.Traverse([this](SceneNode* node, const XMMATRIX& worldMatrix) {
        Mesh* mesh = node->GetMesh();
        if (!mesh)
            return;

        // Ensure mesh is uploaded
        UploadMesh(mesh);

        auto it = m_meshCache.find(mesh);
        if (it == m_meshCache.end())
            return;

        // Transpose for HLSL column-major layout
        XMMATRIX transposed = XMMatrixTranspose(worldMatrix);
        XMFLOAT4X4 worldFloat;
        XMStoreFloat4x4(&worldFloat, transposed);

        m_context->DrawPrimitives(it->second.vb.get(), it->second.ib.get(), worldFloat);
    });
}

void Renderer::RenderLightIndicator(PointLight* light, bool show,
    IRHIBuffer* sphereVB, IRHIBuffer* sphereIB)
{
    if (!m_context || !show || !light || !sphereVB || !sphereIB)
        return;

    XMFLOAT3 lp = light->GetPosition();
    XMMATRIX lightWorld = XMMatrixTranspose(
        XMMatrixScaling(0.06f, 0.06f, 0.06f) * XMMatrixTranslation(lp.x, lp.y, lp.z));
    XMFLOAT4X4 lightWorldFloat;
    XMStoreFloat4x4(&lightWorldFloat, lightWorld);

    m_context->SetUnlitMode(true, light->GetColor());
    m_context->DrawPrimitives(sphereVB, sphereIB, lightWorldFloat);
    m_context->SetUnlitMode(false, { 1.0f, 1.0f, 1.0f });
}

} // namespace RRE
