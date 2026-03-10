#include "Renderer/Renderer.h"
#include "Renderer/Mesh.h"
#include "Renderer/Vertex.h"
#include "RHI/D3D12/D3D12Context.h"
#include "RHI/D3D12/D3D12Buffer.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneNode.h"
#include "Scene/Camera.h"
#include "Lighting/LightManager.h"
#include "Lighting/Light.h"
#include "Asset/Material.h"
#include "Asset/TextureCache.h"
#include <DirectXMath.h>
#include <d3d12.h>
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace RRE
{

void Renderer::SetContext(D3D12Context* context, ID3D12Device* device)
{
    m_context   = context;
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

void Renderer::SetMipMappingEnabled(bool enabled)
{
    m_mipMappingEnabled = enabled;
    if (m_context)
        m_context->SetMipMappingEnabled(enabled);
}

void Renderer::ClearMeshCache()
{
    m_meshCache.clear();      // frees GPU buffers
    m_lodSelector.Clear();    // frees generated LOD meshes (after GPU cache cleared)
}

void Renderer::RegisterMeshesForLOD(
    const std::vector<std::unique_ptr<Mesh>>& meshes,
    float sceneDiagonal)
{
    for (const auto& mesh : meshes)
    {
        if (!mesh) continue;
        // LOD switch distances scale with the scene so transitions feel natural.
        // Multipliers are intentionally large so LOD changes are barely noticeable during
        // normal navigation and only kick in when the camera is very far from an object.
        float dist1 = sceneDiagonal * 2.0f;
        float dist2 = sceneDiagonal * 6.0f;
        m_lodSelector.RegisterMesh(mesh.get(), dist1, dist2);
    }
}

void Renderer::RenderScene(SceneGraph& graph, Camera& camera,
    float aspectRatio, LightManager* lightManager)
{
    if (!m_context)
        return;

    // -----------------------------------------------------------------------
    // Build world-space frustum for this frame
    // -----------------------------------------------------------------------
    XMMATRIX view       = camera.GetViewMatrix();
    XMMATRIX projection = camera.GetProjectionMatrix(aspectRatio);
    m_frustumCuller.Build(view, projection);

    XMMATRIX viewProj = view * projection;

    XMFLOAT3 camPos = camera.GetPosition();

    // -----------------------------------------------------------------------
    // Light culling — discard lights that cannot contribute to visible pixels
    // -----------------------------------------------------------------------
    std::vector<uint32_t> activeLightIndices;
    if (lightManager && lightManager->GetActiveLightCount() > 0)
    {
        if (m_lightCullingEnabled)
        {
            activeLightIndices = m_lightCuller.CullLights(
                m_frustumCuller.GetFrustum(), camPos, *lightManager);
        }
        else
        {
            // Light culling disabled: pass all lights to GPU
            uint32_t count = static_cast<uint32_t>(lightManager->GetActiveLightCount());
            activeLightIndices.resize(count);
            for (uint32_t i = 0; i < count; i++) activeLightIndices[i] = i;
        }
    }

    // -----------------------------------------------------------------------
    // Set GPU rendering state
    // -----------------------------------------------------------------------
    XMFLOAT4X4 viewProjFloat;
    XMStoreFloat4x4(&viewProjFloat, XMMatrixTranspose(viewProj));
    m_context->SetViewProjection(viewProjFloat);

    // Legacy single-light path (BasicColor shader)
    if (!activeLightIndices.empty())
    {
        const Light& firstLight = lightManager->GetLight(activeLightIndices[0]);
        XMFLOAT3 ambient = { 0.15f, 0.15f, 0.15f };
        m_context->SetLightData(
            firstLight.position, firstLight.color,
            camPos, ambient,
            firstLight.Kc, firstLight.Kl, firstLight.Kq);
    }

    // PBR multi-light path (filtered by light culling)
    LightConstants builtLights = {};
    if (lightManager && !activeLightIndices.empty())
    {
        builtLights = lightManager->BuildFilteredLightConstants(activeLightIndices);
        m_context->SetPBRLightData(builtLights);
    }

    m_context->SetRenderModeInt(static_cast<int>(m_renderMode));

    // -----------------------------------------------------------------------
    // Shadow depth pass (FullPBRShadows mode only)
    // -----------------------------------------------------------------------
    ShadowConstants shadowConst = {};
    uint32 shadowCasterCount = lightManager ? lightManager->GetShadowCasterCount() : 0;

    if (shadowCasterCount > 0 && m_renderMode == RenderMode::FullPBRShadows)
    {
        m_context->CreateShadowMaps();

        uint32 shadowIdx = 0;
        for (uint32 li = 0; li < builtLights.numActiveLights && shadowIdx < MAX_SHADOW_MAPS; li++)
        {
            if (builtLights.lights[li].shadowMapIndex < 0)
                continue;

            const GPULightData& gpuLight = builtLights.lights[li];
            XMMATRIX lvp;

            if (gpuLight.type == 0)  // Directional
            {
                XMVECTOR dir    = XMLoadFloat3(&gpuLight.direction);
                XMVECTOR center = XMLoadFloat3(&m_sceneCenter);
                XMVECTOR up     = XMVectorSet(0, 1, 0, 0);
                if (XMVector3NearEqual(XMVector3Normalize(dir), XMVectorSet(0,-1,0,0), XMVectorReplicate(0.01f)) ||
                    XMVector3NearEqual(XMVector3Normalize(dir), XMVectorSet(0, 1,0,0), XMVectorReplicate(0.01f)))
                    up = XMVectorSet(0, 0, 1, 0);
                float orthoSize = m_sceneDiagonal * 1.5f;
                float farPlane  = m_sceneDiagonal * 3.0f;
                // nearPlane scales with scene so small models (e.g. FlightHelmet, diagonal~0.66)
                // don't get their casters clipped.  Shadow cam is diagonal*1.5 from center;
                // scene geometry starts at diagonal*1.0 from cam, so diagonal*0.5 gives 50%
                // clearance and keeps near/far ratio at 6:1 (good D32_FLOAT precision).
                float nearPlane = m_sceneDiagonal * 0.5f;
                // Place shadow camera behind the scene so the entire depth range [near, far]
                // covers the scene.  Camera at sceneCenter - dir*(farPlane/2),
                // looking toward sceneCenter + dir*(farPlane/2).
                XMVECTOR shadowCamPos = XMVectorSubtract(center,
                    XMVectorScale(dir, farPlane * 0.5f));
                lvp = XMMatrixLookAtLH(shadowCamPos, XMVectorAdd(shadowCamPos, dir), up)
                    * XMMatrixOrthographicLH(orthoSize, orthoSize, nearPlane, farPlane);
            }
            else if (gpuLight.type == 2)  // Spot
            {
                XMVECTOR dir = XMLoadFloat3(&gpuLight.direction);
                XMVECTOR pos = XMLoadFloat3(&gpuLight.position);
                XMVECTOR up  = XMVectorSet(0, 1, 0, 0);
                if (XMVector3NearEqual(XMVector3Normalize(dir), XMVectorSet(0,-1,0,0), XMVectorReplicate(0.01f)) ||
                    XMVector3NearEqual(XMVector3Normalize(dir), XMVectorSet(0, 1,0,0), XMVectorReplicate(0.01f)))
                    up = XMVectorSet(0, 0, 1, 0);
                float fov = acosf(gpuLight.outerConeAngle) * 2.0f;
                if (fov < 0.1f) fov = 0.1f;
                float spotFar  = m_sceneDiagonal * 3.0f;
                float spotNear = m_sceneDiagonal * 0.05f;  // scale with scene; was hardcoded 0.1f
                lvp = XMMatrixLookAtLH(pos, XMVectorAdd(pos, dir), up)
                    * XMMatrixPerspectiveFovLH(fov, 1.0f, spotNear, spotFar);
            }
            else  // Point light (cube shadow not yet implemented)
            {
                shadowIdx++;
                continue;
            }

            XMStoreFloat4x4(&shadowConst.lightViewProj[shadowIdx], XMMatrixTranspose(lvp));

            XMFLOAT4X4 lvpFloat;
            XMStoreFloat4x4(&lvpFloat, XMMatrixTranspose(lvp));

            m_context->BeginShadowPass(shadowIdx);
            graph.Traverse([this, &lvpFloat](SceneNode* node, const XMMATRIX& worldMatrix)
            {
                Mesh* mesh = node->GetMesh();
                if (!mesh) return;
                UploadMesh(mesh);
                auto it = m_meshCache.find(mesh);
                if (it == m_meshCache.end()) return;

                XMFLOAT4X4 worldFloat;
                XMStoreFloat4x4(&worldFloat, XMMatrixTranspose(worldMatrix));
                m_context->DrawShadowDepth(
                    it->second.vb.get(), it->second.ib.get(), worldFloat, lvpFloat);
            });
            m_context->EndShadowPass(shadowIdx);
            shadowIdx++;
        }
        shadowConst.shadowMapCount = shadowIdx;

        // Restore main back-buffer RTV/DSV/viewport after all shadow passes
        m_context->RestoreMainRenderTarget();
    }
    shadowConst.shadowTexelSize = 1.0f / static_cast<float>(m_context->GetShadowMapSize());
    // Normal-offset bias: shift shadow sampling by 2 world-space texels along surface normal.
    // Prevents PCF samples from crossing geometry discontinuities (e.g. base top → base front face).
    // Scales with orthoSize (= diagonal * 1.5) so it works across small and large scenes.
    shadowConst.shadowNormalBiasWorld =
        (m_sceneDiagonal * 1.5f) / static_cast<float>(m_context->GetShadowMapSize()) * 2.0f;
    m_context->SetShadowData(shadowConst);

    // -----------------------------------------------------------------------
    // Reset per-frame stats
    // -----------------------------------------------------------------------
    m_lastCullStats = {};
    m_lastCullStats.activeLights = static_cast<uint32>(activeLightIndices.size());
    if (lightManager)
    {
        uint32 totalLights = static_cast<uint32>(lightManager->GetActiveLightCount());
        m_lastCullStats.culledLights = totalLights > m_lastCullStats.activeLights
            ? totalLights - m_lastCullStats.activeLights : 0u;
    }

    // -----------------------------------------------------------------------
    // Pass 1: Opaque + Alpha Mask (or Wireframe)
    // -----------------------------------------------------------------------
    graph.Traverse([this, &camPos, &viewProj](
        SceneNode* node, const XMMATRIX& worldMatrix)
    {
        Mesh* mesh = node->GetMesh();
        if (!mesh) return;

        // --- Frustum culling ---
        DirectX::BoundingBox worldAABB = node->GetWorldAABB();
        if (m_frustumCullingEnabled && !m_frustumCuller.IsVisible(worldAABB))
        {
            m_lastCullStats.frustumCulledNodes++;
            return;
        }

        // --- Occlusion culling (P0 stub: never culls) ---
        if (m_occlusionCuller.IsOccluded(worldAABB, viewProj))
        {
            m_lastCullStats.occlusionCulledNodes++;
            return;
        }

        // --- LOD selection (Pass 1) ---
        XMVECTOR objCenter = XMLoadFloat3(&worldAABB.Center);
        XMVECTOR camV      = XMLoadFloat3(&camPos);
        float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(objCenter, camV)));
        Mesh* drawMesh = m_lodEnabled ? m_lodSelector.SelectLOD(mesh, dist) : mesh;

        Material* material = node->GetMaterial();

        // Alpha-blend objects go in pass 2
        if (material && material->alphaMode == AlphaMode::Blend
            && m_renderMode != RenderMode::Wireframe)
            return;

        UploadMesh(drawMesh);
        auto it = m_meshCache.find(drawMesh);
        if (it == m_meshCache.end()) return;

        XMFLOAT4X4 worldFloat;
        XMStoreFloat4x4(&worldFloat, XMMatrixTranspose(worldMatrix));

        if (m_renderMode == RenderMode::Wireframe)
        {
            m_context->DrawPrimitivesWireframe(
                it->second.vb.get(), it->second.ib.get(), worldFloat);
        }
        else if (material && m_textureCache)
        {
            m_context->DrawPrimitivesPBR(
                it->second.vb.get(), it->second.ib.get(),
                worldFloat, material, m_textureCache);
        }
        else
        {
            m_context->DrawPrimitives(
                it->second.vb.get(), it->second.ib.get(), worldFloat);
        }

        m_lastCullStats.renderedPolygons += drawMesh->GetPolygonCount();
        m_lastCullStats.visibleNodes++;
    });

    // -----------------------------------------------------------------------
    // Pass 2: Alpha Blend, back-to-front sorted (skip in Wireframe)
    // -----------------------------------------------------------------------
    if (m_renderMode != RenderMode::Wireframe)
    {
        struct BlendDC
        {
            SceneNode* node;
            Mesh*      drawMesh;
            XMFLOAT4X4 worldFloat;
            float      distSq;
        };
        std::vector<BlendDC> blendList;

        graph.Traverse([this, &camPos, &viewProj, &blendList](
            SceneNode* node, const XMMATRIX& worldMatrix)
        {
            Mesh* mesh = node->GetMesh();
            if (!mesh) return;

            Material* material = node->GetMaterial();
            if (!material || material->alphaMode != AlphaMode::Blend) return;

            DirectX::BoundingBox worldAABB = node->GetWorldAABB();
            if (m_frustumCullingEnabled && !m_frustumCuller.IsVisible(worldAABB)) return;
            if (m_occlusionCuller.IsOccluded(worldAABB, viewProj)) return;

            XMVECTOR objCenter = XMLoadFloat3(&worldAABB.Center);
            XMVECTOR camV      = XMLoadFloat3(&camPos);
            XMVECTOR diff      = XMVectorSubtract(objCenter, camV);
            float distSq = XMVectorGetX(XMVector3Dot(diff, diff));
            float dist   = sqrtf(distSq);

            // --- LOD selection (Pass 2) ---
            Mesh* drawMesh = m_lodEnabled ? m_lodSelector.SelectLOD(mesh, dist) : mesh;
            UploadMesh(drawMesh);
            if (m_meshCache.find(drawMesh) == m_meshCache.end()) return;

            XMFLOAT4X4 worldFloat;
            XMStoreFloat4x4(&worldFloat, XMMatrixTranspose(worldMatrix));
            blendList.push_back({ node, drawMesh, worldFloat, distSq });
        });

        std::sort(blendList.begin(), blendList.end(),
            [](const BlendDC& a, const BlendDC& b) { return a.distSq > b.distSq; });

        for (auto& dc : blendList)
        {
            auto it = m_meshCache.find(dc.drawMesh);
            if (it == m_meshCache.end()) continue;

            m_context->DrawPrimitivesPBR(
                it->second.vb.get(), it->second.ib.get(),
                dc.worldFloat, dc.node->GetMaterial(), m_textureCache);

            m_lastCullStats.renderedPolygons += dc.drawMesh->GetPolygonCount();
            m_lastCullStats.visibleNodes++;
        }
    }
}

} // namespace RRE
