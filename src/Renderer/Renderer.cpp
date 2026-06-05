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

void Renderer::SetOcclusionCullingEnabled(bool enabled)
{
    m_occlusionCullingEnabled = enabled;
    if (!enabled)
    {
        m_occlusionCuller.ClearResults();
        m_hizNodeList.clear();
        m_hizNodeOrder.clear();
    }
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
    // Hi-Z Occlusion Culling: read results from previous frame (1-frame latency)
    // -----------------------------------------------------------------------
    if (m_occlusionCullingEnabled)
    {
        uint32 nodeCount = 0;
        const uint32* results = m_context->ReadOcclusionResults(nodeCount);
        if (results && nodeCount > 0 && nodeCount <= m_hizNodeOrder.size())
            m_occlusionCuller.UpdateResults(m_hizNodeOrder.data(), results, nodeCount);
    }
    m_hizNodeList.clear();   // collect fresh list this frame

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

        // Camera axes for CSM cascade frustum corner computation
        XMMATRIX camView    = camera.GetViewMatrix();
        XMMATRIX camInvView = XMMatrixInverse(nullptr, camView);
        XMVECTOR camRight   = XMVector3Normalize(camInvView.r[0]);
        XMVECTOR camUp_     = XMVector3Normalize(camInvView.r[1]);
        XMVECTOR camFwd     = XMVector3Normalize(camInvView.r[2]);
        XMVECTOR camPosV    = camInvView.r[3];
        float    tanHalfFov = tanf(camera.GetFov() * 0.5f);

        uint32 shadowIdx = 0;
        for (uint32 li = 0; li < builtLights.numActiveLights && shadowIdx < MAX_SHADOW_MAPS; li++)
        {
            if (builtLights.lights[li].shadowMapIndex < 0)
                continue;

            const GPULightData& gpuLight = builtLights.lights[li];

            if (gpuLight.type == 0 && m_csmEnabled)
            {
                // ----------------------------------------------------------------
                // CSM — 3-cascade Directional shadow
                // ----------------------------------------------------------------
                if (shadowIdx + CSM_NUM_CASCADES > MAX_SHADOW_MAPS)
                    break;

                XMFLOAT3 camFwdF;
                XMStoreFloat3(&camFwdF, camFwd);
                shadowConst.cameraForward = camFwdF;
                shadowConst.csmEnabled    = 1;

                // Cascade split depths (Practical Split Scheme, lambda = 0.5)
                float nearZ  = camera.GetNearPlane();
                float farZ   = min(camera.GetFarPlane(), m_sceneDiagonal * 3.0f);
                float lambda = 0.5f;

                float splitDepths[CSM_NUM_CASCADES + 1];
                splitDepths[0]                = nearZ;
                splitDepths[CSM_NUM_CASCADES] = farZ;
                for (uint32 k = 1; k < CSM_NUM_CASCADES; k++)
                {
                    float p         = static_cast<float>(k) / CSM_NUM_CASCADES;
                    float logSplit  = nearZ * powf(farZ / nearZ, p);
                    float unifSplit = nearZ + (farZ - nearZ) * p;
                    splitDepths[k]  = lambda * logSplit + (1.0f - lambda) * unifSplit;
                }
                shadowConst.cascadeSplitDepths = {
                    splitDepths[1], splitDepths[2], splitDepths[3]
                };

                // Stable light view matrix (shared across all cascades)
                XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&gpuLight.direction));
                XMVECTOR up  = XMVectorSet(0, 1, 0, 0);
                if (XMVector3NearEqual(dir, XMVectorSet(0,-1,0,0), XMVectorReplicate(0.01f)) ||
                    XMVector3NearEqual(dir, XMVectorSet(0, 1,0,0), XMVectorReplicate(0.01f)))
                    up = XMVectorSet(0, 0, 1, 0);
                XMMATRIX lightView = XMMatrixLookAtLH(
                    XMVectorNegate(dir) * 1000.0f, XMVectorZero(), up);

                for (uint32 cascade = 0; cascade < CSM_NUM_CASCADES; cascade++)
                {
                    float cascNear = splitDepths[cascade];
                    float cascFar  = splitDepths[cascade + 1];

                    float nearHW = cascNear * tanHalfFov * aspectRatio;
                    float nearHH = cascNear * tanHalfFov;
                    float farHW  = cascFar  * tanHalfFov * aspectRatio;
                    float farHH  = cascFar  * tanHalfFov;

                    XMVECTOR nearC = XMVectorAdd(camPosV, XMVectorScale(camFwd, cascNear));
                    XMVECTOR farC  = XMVectorAdd(camPosV, XMVectorScale(camFwd, cascFar));

                    XMVECTOR corners[8] = {
                        XMVectorAdd(XMVectorAdd(nearC, XMVectorScale(camRight,-nearHW)), XMVectorScale(camUp_,-nearHH)),
                        XMVectorAdd(XMVectorAdd(nearC, XMVectorScale(camRight, nearHW)), XMVectorScale(camUp_,-nearHH)),
                        XMVectorAdd(XMVectorAdd(nearC, XMVectorScale(camRight,-nearHW)), XMVectorScale(camUp_, nearHH)),
                        XMVectorAdd(XMVectorAdd(nearC, XMVectorScale(camRight, nearHW)), XMVectorScale(camUp_, nearHH)),
                        XMVectorAdd(XMVectorAdd(farC,  XMVectorScale(camRight,-farHW)),  XMVectorScale(camUp_,-farHH)),
                        XMVectorAdd(XMVectorAdd(farC,  XMVectorScale(camRight, farHW)),  XMVectorScale(camUp_,-farHH)),
                        XMVectorAdd(XMVectorAdd(farC,  XMVectorScale(camRight,-farHW)),  XMVectorScale(camUp_, farHH)),
                        XMVectorAdd(XMVectorAdd(farC,  XMVectorScale(camRight, farHW)),  XMVectorScale(camUp_, farHH)),
                    };

                    // Transform corners to light view space → tight AABB
                    float minX = FLT_MAX, maxX = -FLT_MAX;
                    float minY = FLT_MAX, maxY = -FLT_MAX;
                    float minZ = FLT_MAX, maxZ = -FLT_MAX;
                    for (int ci = 0; ci < 8; ci++)
                    {
                        XMVECTOR lv = XMVector3Transform(corners[ci], lightView);
                        XMFLOAT3 lvf; XMStoreFloat3(&lvf, lv);
                        minX = min(minX, lvf.x); maxX = max(maxX, lvf.x);
                        minY = min(minY, lvf.y); maxY = max(maxY, lvf.y);
                        minZ = min(minZ, lvf.z); maxZ = max(maxZ, lvf.z);
                    }

                    // Pull Z back to catch shadow casters behind the cascade slice
                    float zExpand = (maxZ - minZ) * 0.5f + m_sceneDiagonal * 0.1f;
                    minZ -= zExpand;
                    // Small XY margin to avoid edge seam artifacts
                    float mX = (maxX - minX) * 0.02f;
                    float mY = (maxY - minY) * 0.02f;
                    minX -= mX; maxX += mX;
                    minY -= mY; maxY += mY;

                    XMMATRIX shadowProj = XMMatrixOrthographicOffCenterLH(
                        minX, maxX, minY, maxY, minZ, maxZ);
                    XMMATRIX lvp = lightView * shadowProj;

                    uint32 slotIdx = shadowIdx + cascade;
                    XMStoreFloat4x4(&shadowConst.lightViewProj[slotIdx], XMMatrixTranspose(lvp));

                    XMFLOAT4X4 lvpFloat;
                    XMStoreFloat4x4(&lvpFloat, XMMatrixTranspose(lvp));

                    FrustumCuller cascadeFrustum;
                    cascadeFrustum.Build(lightView, shadowProj);

                    m_context->BeginShadowPass(slotIdx);
                    graph.Traverse([this, &lvpFloat, &cascadeFrustum](
                        SceneNode* node, const XMMATRIX& worldMatrix)
                    {
                        Mesh* mesh = node->GetMesh();
                        if (!mesh) return;
                        if (m_frustumCullingEnabled &&
                            !cascadeFrustum.IsVisible(node->GetWorldAABB()))
                            return;
                        UploadMesh(mesh);
                        auto it = m_meshCache.find(mesh);
                        if (it == m_meshCache.end()) return;
                        XMFLOAT4X4 worldFloat;
                        XMStoreFloat4x4(&worldFloat, XMMatrixTranspose(worldMatrix));
                        m_context->DrawShadowDepth(
                            it->second.vb.get(), it->second.ib.get(), worldFloat, lvpFloat);
                    });
                    m_context->EndShadowPass(slotIdx);
                }
                shadowIdx += CSM_NUM_CASCADES;
            }
            else if (gpuLight.type == 0 || gpuLight.type == 2)
            {
                // ----------------------------------------------------------------
                // Single shadow map — Directional (CSM off) or Spot
                // ----------------------------------------------------------------
                XMMATRIX shadowView = XMMatrixIdentity();
                XMMATRIX shadowProj = XMMatrixIdentity();
                XMMATRIX lvp;

                if (gpuLight.type == 0)  // Directional (CSM disabled)
                {
                    XMVECTOR dir    = XMLoadFloat3(&gpuLight.direction);
                    XMVECTOR center = XMLoadFloat3(&m_sceneCenter);
                    XMVECTOR up     = XMVectorSet(0, 1, 0, 0);
                    if (XMVector3NearEqual(XMVector3Normalize(dir), XMVectorSet(0,-1,0,0), XMVectorReplicate(0.01f)) ||
                        XMVector3NearEqual(XMVector3Normalize(dir), XMVectorSet(0, 1,0,0), XMVectorReplicate(0.01f)))
                        up = XMVectorSet(0, 0, 1, 0);
                    float orthoSize = m_sceneDiagonal * 1.5f;
                    float farPlane  = m_sceneDiagonal * 3.0f;
                    float nearPlane = m_sceneDiagonal * 0.5f;
                    XMVECTOR shadowCamPos = XMVectorSubtract(center,
                        XMVectorScale(dir, farPlane * 0.5f));
                    shadowView = XMMatrixLookAtLH(shadowCamPos, XMVectorAdd(shadowCamPos, dir), up);
                    shadowProj = XMMatrixOrthographicLH(orthoSize, orthoSize, nearPlane, farPlane);
                    lvp = shadowView * shadowProj;
                }
                else  // Spot
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
                    float spotNear = m_sceneDiagonal * 0.05f;
                    shadowView = XMMatrixLookAtLH(pos, XMVectorAdd(pos, dir), up);
                    shadowProj = XMMatrixPerspectiveFovLH(fov, 1.0f, spotNear, spotFar);
                    lvp = shadowView * shadowProj;
                }

                XMStoreFloat4x4(&shadowConst.lightViewProj[shadowIdx], XMMatrixTranspose(lvp));

                XMFLOAT4X4 lvpFloat;
                XMStoreFloat4x4(&lvpFloat, XMMatrixTranspose(lvp));

                FrustumCuller lightFrustum;
                lightFrustum.Build(shadowView, shadowProj);

                m_context->BeginShadowPass(shadowIdx);
                graph.Traverse([this, &lvpFloat, &lightFrustum](
                    SceneNode* node, const XMMATRIX& worldMatrix)
                {
                    Mesh* mesh = node->GetMesh();
                    if (!mesh) return;
                    if (m_frustumCullingEnabled &&
                        !lightFrustum.IsVisible(node->GetWorldAABB()))
                        return;
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
            // Point light: cube shadow not yet implemented — skip
        }
        shadowConst.shadowMapCount = shadowIdx;
        shadowConst.csmDebugView   = m_csmDebugView ? 1u : 0u;
        shadowConst.pcssEnabled    = m_pcssEnabled  ? 1u : 0u;
        shadowConst.lightSize      = m_sceneDiagonal * 0.02f * m_lightSizeMultiplier;

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
    // Pass 1: Collect visible Opaque + Alpha Mask nodes, group into batches
    // -----------------------------------------------------------------------
    m_opaqueBatcher.Clear();
    m_alphaMaskBatcher.Clear();

    // Wireframe mode: draw immediately without batching (single-instance draw calls)
    if (m_renderMode == RenderMode::Wireframe)
    {
        graph.Traverse([this, &camPos, &viewProj](
            SceneNode* node, const XMMATRIX& worldMatrix)
        {
            Mesh* mesh = node->GetMesh();
            if (!mesh) return;

            DirectX::BoundingBox worldAABB = node->GetWorldAABB();
            if (m_frustumCullingEnabled && !m_frustumCuller.IsVisible(worldAABB))
            { m_lastCullStats.frustumCulledNodes++; return; }
            if (m_occlusionCullingEnabled) m_hizNodeList.push_back({ node, worldAABB });
            if (m_occlusionCullingEnabled && m_occlusionCuller.IsOccluded(node))
            { m_lastCullStats.occlusionCulledNodes++; return; }

            XMVECTOR objCenter = XMLoadFloat3(&worldAABB.Center);
            XMVECTOR camV      = XMLoadFloat3(&camPos);
            float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(objCenter, camV)));
            float effectiveDist = (m_vramPressure && m_lodEnabled) ? dist * 2.0f : dist;
            Mesh* drawMesh = m_lodEnabled ? m_lodSelector.SelectLOD(mesh, effectiveDist) : mesh;

            UploadMesh(drawMesh);
            auto it = m_meshCache.find(drawMesh);
            if (it == m_meshCache.end()) return;

            XMFLOAT4X4 worldFloat;
            XMStoreFloat4x4(&worldFloat, XMMatrixTranspose(worldMatrix));
            m_context->DrawPrimitivesWireframe(
                it->second.vb.get(), it->second.ib.get(), worldFloat);

            m_lastCullStats.renderedPolygons += drawMesh->GetPolygonCount();
            m_lastCullStats.visibleNodes++;
        });
    }
    else
    {
        // PBR modes: collect into batchers, then issue instanced draw calls
        graph.Traverse([this, &camPos, &viewProj](
            SceneNode* node, const XMMATRIX& worldMatrix)
        {
            Mesh* mesh = node->GetMesh();
            if (!mesh) return;

            DirectX::BoundingBox worldAABB = node->GetWorldAABB();
            if (m_frustumCullingEnabled && !m_frustumCuller.IsVisible(worldAABB))
            { m_lastCullStats.frustumCulledNodes++; return; }
            if (m_occlusionCullingEnabled) m_hizNodeList.push_back({ node, worldAABB });
            if (m_occlusionCullingEnabled && m_occlusionCuller.IsOccluded(node))
            { m_lastCullStats.occlusionCulledNodes++; return; }

            XMVECTOR objCenter = XMLoadFloat3(&worldAABB.Center);
            XMVECTOR camV      = XMLoadFloat3(&camPos);
            float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(objCenter, camV)));
            // VRAM pressure: scale distance up so LOD switches to lower detail sooner
            float effectiveDist = (m_vramPressure && m_lodEnabled) ? dist * 2.0f : dist;
            Mesh* drawMesh = m_lodEnabled ? m_lodSelector.SelectLOD(mesh, effectiveDist) : mesh;

            Material* material = node->GetMaterial();

            // Ensure GPU buffers exist before batching
            UploadMesh(drawMesh);
            if (m_meshCache.find(drawMesh) == m_meshCache.end()) return;

            XMFLOAT4X4 worldFloat;
            XMStoreFloat4x4(&worldFloat, XMMatrixTranspose(worldMatrix));

            if (material && material->alphaMode == AlphaMode::Blend)
                return;  // Alpha-blend goes in pass 2

            if (material && material->alphaMode == AlphaMode::Mask)
                m_alphaMaskBatcher.AddInstance(drawMesh, material, worldFloat);
            else
                m_opaqueBatcher.AddInstance(drawMesh, material, worldFloat);
        });

        // Sort opaque batches front-to-back to maximize GPU Early-Z rejection
        m_opaqueBatcher.SortFrontToBack(camPos);

        // Record instancing stats before drawing
        m_lastCullStats.opaqueBatches  = static_cast<uint32>(m_opaqueBatcher.GetBatches().size());
        m_lastCullStats.totalInstances = m_opaqueBatcher.GetTotalInstanceCount();

        // --- Draw opaque batches ---
        for (const auto& batch : m_opaqueBatcher.GetBatches())
        {
            auto it = m_meshCache.find(batch.mesh);
            if (it == m_meshCache.end()) continue;

            const auto& worlds = batch.worlds;
            if (batch.material && m_textureCache)
            {
                m_context->DrawPrimitivesPBRInstanced(
                    it->second.vb.get(), it->second.ib.get(),
                    worlds.data(), static_cast<uint32>(worlds.size()),
                    batch.material, m_textureCache);
            }
            else
            {
                for (const auto& w : worlds)
                    m_context->DrawPrimitives(it->second.vb.get(), it->second.ib.get(), w);
            }

            m_lastCullStats.renderedPolygons +=
                batch.mesh->GetPolygonCount() * static_cast<uint32>(worlds.size());
            m_lastCullStats.visibleNodes += static_cast<uint32>(worlds.size());
        }

        // --- Draw alpha-mask batches ---
        for (const auto& batch : m_alphaMaskBatcher.GetBatches())
        {
            auto it = m_meshCache.find(batch.mesh);
            if (it == m_meshCache.end()) continue;

            const auto& worlds = batch.worlds;
            if (batch.material && m_textureCache)
            {
                m_context->DrawPrimitivesPBRInstanced(
                    it->second.vb.get(), it->second.ib.get(),
                    worlds.data(), static_cast<uint32>(worlds.size()),
                    batch.material, m_textureCache);
            }

            m_lastCullStats.renderedPolygons +=
                batch.mesh->GetPolygonCount() * static_cast<uint32>(worlds.size());
            m_lastCullStats.visibleNodes += static_cast<uint32>(worlds.size());
        }
    }

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
            if (m_occlusionCullingEnabled && m_occlusionCuller.IsOccluded(node)) return;

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

    // -----------------------------------------------------------------------
    // Hi-Z Occlusion Culling: dispatch GPU test for next frame (1-frame latency)
    // -----------------------------------------------------------------------
    if (m_occlusionCullingEnabled && m_context && !m_hizNodeList.empty())
    {
        uint32 count = (std::min)(
            static_cast<uint32>(m_hizNodeList.size()),
            static_cast<uint32>(D3D12Context::MAX_OCCLUSION_NODES));

        // Build GPU AABB array and matching node order
        std::vector<GPUOcclusionAABB> aabbs;
        aabbs.reserve(count);
        m_hizNodeOrder.clear();
        m_hizNodeOrder.reserve(count);

        for (uint32 i = 0; i < count; i++)
        {
            const auto& hn = m_hizNodeList[i];
            GPUOcclusionAABB a;
            a.centerX = hn.aabb.Center.x;
            a.centerY = hn.aabb.Center.y;
            a.centerZ = hn.aabb.Center.z;
            a.pad0    = 0.0f;
            a.extentX = hn.aabb.Extents.x;
            a.extentY = hn.aabb.Extents.y;
            a.extentZ = hn.aabb.Extents.z;
            a.pad1    = 0.0f;
            aabbs.push_back(a);
            m_hizNodeOrder.push_back(hn.node);
        }

        // Upload transposed viewProj (same convention as graphics CBVs)
        XMFLOAT4X4 vpT;
        XMStoreFloat4x4(&vpT, XMMatrixTranspose(viewProj));

        m_context->BuildHiZAndDispatchOcclusionTest(aabbs.data(), count, vpT);
    }
}

} // namespace RRE
