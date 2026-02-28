#include "Renderer/Renderer.h"
#include "Renderer/Mesh.h"
#include "Renderer/Vertex.h"
#include "RHI/D3D12/D3D12Context.h"
#include "RHI/D3D12/D3D12Buffer.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneNode.h"
#include "Scene/Camera.h"
#include "Lighting/PointLight.h"
#include "Lighting/LightManager.h"
#include "Asset/Material.h"
#include "Asset/TextureCache.h"
#include <DirectXMath.h>
#include <d3d12.h>
#include <cmath>
#include <algorithm>

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
    float aspectRatio, LightManager* lightManager)
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

    // Set lighting data (BasicColor path)
    XMFLOAT3 camPos = camera.GetPosition();
    if (light)
    {
        XMFLOAT3 ambient = { 0.15f, 0.15f, 0.15f };
        m_context->SetLightData(
            light->GetPosition(), light->GetColor(),
            camPos, ambient,
            light->GetConstantAttenuation(),
            light->GetLinearAttenuation(),
            light->GetQuadraticAttenuation());
    }

    // Set PBR light data from LightManager (or fallback to PointLight)
    LightConstants builtLights = {};
    if (lightManager && lightManager->GetActiveLightCount() > 0)
    {
        builtLights = lightManager->BuildLightConstants();
        m_context->SetPBRLightData(builtLights);
    }
    else if (light)
    {
        builtLights.numActiveLights = 1;
        builtLights.lights[0].position = light->GetPosition();
        builtLights.lights[0].color = light->GetColor();
        builtLights.lights[0].intensity = 1.0f;
        builtLights.lights[0].type = 1; // Point
        builtLights.lights[0].Kc = light->GetConstantAttenuation();
        builtLights.lights[0].Kl = light->GetLinearAttenuation();
        builtLights.lights[0].Kq = light->GetQuadraticAttenuation();
        builtLights.lights[0].shadowMapIndex = -1;
        m_context->SetPBRLightData(builtLights);
    }

    // Pass render mode to D3D12Context for texture flag overrides
    m_context->SetRenderModeInt(static_cast<int>(m_renderMode));

    // --- Shadow Depth Pass (only in FullPBRShadows mode) ---
    ShadowConstants shadowConst = {};
    uint32 shadowCasterCount = lightManager ? lightManager->GetShadowCasterCount() : 0;

    if (shadowCasterCount > 0 && m_renderMode == RenderMode::FullPBRShadows)
    {
        m_context->CreateShadowMaps();

        // Build light-view-projection matrices and render shadow depth
        uint32 shadowIdx = 0;
        for (uint32 li = 0; li < builtLights.numActiveLights && shadowIdx < MAX_SHADOW_MAPS; li++)
        {
            if (builtLights.lights[li].shadowMapIndex < 0)
                continue;

            const GPULightData& gpuLight = builtLights.lights[li];
            XMMATRIX lvp;

            if (gpuLight.type == 0)
            {
                // Directional: orthographic projection
                XMVECTOR dir = XMLoadFloat3(&gpuLight.direction);
                XMVECTOR pos = XMLoadFloat3(&gpuLight.position);
                XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
                // Avoid degenerate up vector if light points straight down/up
                if (XMVector3NearEqual(XMVector3Normalize(dir), XMVectorSet(0,-1,0,0), XMVectorReplicate(0.01f)) ||
                    XMVector3NearEqual(XMVector3Normalize(dir), XMVectorSet(0,1,0,0), XMVectorReplicate(0.01f)))
                    up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
                XMMATRIX lightView = XMMatrixLookAtLH(pos, XMVectorAdd(pos, dir), up);
                XMMATRIX lightProj = XMMatrixOrthographicLH(20.0f, 20.0f, 0.1f, 100.0f);
                lvp = lightView * lightProj;
            }
            else if (gpuLight.type == 2)
            {
                // Spot: perspective projection
                XMVECTOR dir = XMLoadFloat3(&gpuLight.direction);
                XMVECTOR pos = XMLoadFloat3(&gpuLight.position);
                XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
                if (XMVector3NearEqual(XMVector3Normalize(dir), XMVectorSet(0,-1,0,0), XMVectorReplicate(0.01f)) ||
                    XMVector3NearEqual(XMVector3Normalize(dir), XMVectorSet(0,1,0,0), XMVectorReplicate(0.01f)))
                    up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
                XMMATRIX lightView = XMMatrixLookAtLH(pos, XMVectorAdd(pos, dir), up);
                float fov = acosf(gpuLight.outerConeAngle) * 2.0f;
                if (fov < 0.1f) fov = 0.1f;
                XMMATRIX lightProj = XMMatrixPerspectiveFovLH(fov, 1.0f, 0.1f, 100.0f);
                lvp = lightView * lightProj;
            }
            else
            {
                // Point light: skip shadow for now (would need cube map)
                shadowIdx++;
                continue;
            }

            // Store transposed LVP for GPU (column-major)
            XMStoreFloat4x4(&shadowConst.lightViewProj[shadowIdx], XMMatrixTranspose(lvp));

            // Render shadow depth pass
            m_context->BeginShadowPass(shadowIdx);

            // Traverse scene and draw all meshes for shadow depth
            // Use non-transposed lvp for the shadow pass CB (DrawShadowDepth transposes internally)
            XMFLOAT4X4 lvpFloat;
            XMStoreFloat4x4(&lvpFloat, XMMatrixTranspose(lvp));

            graph.Traverse([this, &lvpFloat](SceneNode* node, const XMMATRIX& worldMatrix) {
                Mesh* mesh = node->GetMesh();
                if (!mesh)
                    return;

                UploadMesh(mesh);
                auto it = m_meshCache.find(mesh);
                if (it == m_meshCache.end())
                    return;

                XMFLOAT4X4 worldFloat;
                XMStoreFloat4x4(&worldFloat, XMMatrixTranspose(worldMatrix));

                m_context->DrawShadowDepth(it->second.vb.get(), it->second.ib.get(),
                    worldFloat, lvpFloat);
            });

            m_context->EndShadowPass(shadowIdx);
            shadowIdx++;
        }

        shadowConst.shadowMapCount = shadowIdx;
    }

    m_context->SetShadowData(shadowConst);

    // --- Pass 1: Opaque + Alpha Mask (or Wireframe) ---
    graph.Traverse([this, &camPos](SceneNode* node, const XMMATRIX& worldMatrix) {
        Mesh* mesh = node->GetMesh();
        if (!mesh)
            return;

        Material* material = node->GetMaterial();
        // Skip blend materials — they go in pass 2
        if (material && material->alphaMode == AlphaMode::Blend && m_renderMode != RenderMode::Wireframe)
            return;

        UploadMesh(mesh);
        auto it = m_meshCache.find(mesh);
        if (it == m_meshCache.end())
            return;

        XMMATRIX transposed = XMMatrixTranspose(worldMatrix);
        XMFLOAT4X4 worldFloat;
        XMStoreFloat4x4(&worldFloat, transposed);

        if (m_renderMode == RenderMode::Wireframe)
        {
            m_context->DrawPrimitivesWireframe(it->second.vb.get(), it->second.ib.get(), worldFloat);
        }
        else if (material && m_textureCache)
        {
            m_context->DrawPrimitivesPBR(it->second.vb.get(), it->second.ib.get(),
                worldFloat, material, m_textureCache);
        }
        else
        {
            m_context->DrawPrimitives(it->second.vb.get(), it->second.ib.get(), worldFloat);
        }
    });

    // --- Pass 2: Alpha Blend (back-to-front sorted) — skip in Wireframe mode ---
    if (m_renderMode != RenderMode::Wireframe)
    {
        struct BlendDrawCall
        {
            SceneNode* node;
            XMFLOAT4X4 worldFloat;
            float distSq;
        };
        std::vector<BlendDrawCall> blendList;

        graph.Traverse([this, &camPos, &blendList](SceneNode* node, const XMMATRIX& worldMatrix) {
            Mesh* mesh = node->GetMesh();
            if (!mesh)
                return;

            Material* material = node->GetMaterial();
            if (!material || material->alphaMode != AlphaMode::Blend)
                return;

            UploadMesh(mesh);
            if (m_meshCache.find(mesh) == m_meshCache.end())
                return;

            XMFLOAT4X4 worldFloat;
            XMStoreFloat4x4(&worldFloat, XMMatrixTranspose(worldMatrix));

            XMFLOAT3 objPos;
            XMStoreFloat3(&objPos, worldMatrix.r[3]);
            float dx = objPos.x - camPos.x;
            float dy = objPos.y - camPos.y;
            float dz = objPos.z - camPos.z;
            float distSq = dx * dx + dy * dy + dz * dz;

            blendList.push_back({ node, worldFloat, distSq });
        });

        // Sort back-to-front (farthest first)
        std::sort(blendList.begin(), blendList.end(),
            [](const BlendDrawCall& a, const BlendDrawCall& b) { return a.distSq > b.distSq; });

        for (auto& dc : blendList)
        {
            Mesh* mesh = dc.node->GetMesh();
            auto it = m_meshCache.find(mesh);
            if (it == m_meshCache.end())
                continue;

            m_context->DrawPrimitivesPBR(it->second.vb.get(), it->second.ib.get(),
                dc.worldFloat, dc.node->GetMaterial(), m_textureCache);
        }
    }
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
