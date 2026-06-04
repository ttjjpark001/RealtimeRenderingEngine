// GPU Occlusion Test Compute Shader (Phase 32)
// Tests each AABB against the Hi-Z buffer, writing 0=visible or 1=occluded per node.
// 1-frame latency: results dispatched in frame N are read by CPU in frame N+1.
// Uses conservative MAX-filter Hi-Z: an object is culled only if its nearest NDC depth
// exceeds the Hi-Z MAX in the sampled region (guaranteed behind all occluders).

struct NodeAABB
{
    float3 center;
    float  pad0;
    float3 extents;
    float  pad1;
};

cbuffer OcclusionCB : register(b0)
{
    float4x4 viewProj;      // transposed on CPU (XMMatrixTranspose) — column-major HLSL
    uint     nodeCount;
    uint     screenWidth;
    uint     screenHeight;
    float    _pad;
};

StructuredBuffer<NodeAABB>   g_aabbs   : register(t0);
Texture2D<float>             g_hiz     : register(t1);
RWStructuredBuffer<uint>     g_results : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    uint idx = tid.x;
    if (idx >= nodeCount)
        return;

    NodeAABB aabb = g_aabbs[idx];
    float3 c = aabb.center;
    float3 e = aabb.extents;

    // 8 AABB corners in world space
    float3 corners[8] = {
        c + float3(-e.x, -e.y, -e.z),
        c + float3( e.x, -e.y, -e.z),
        c + float3(-e.x,  e.y, -e.z),
        c + float3( e.x,  e.y, -e.z),
        c + float3(-e.x, -e.y,  e.z),
        c + float3( e.x, -e.y,  e.z),
        c + float3(-e.x,  e.y,  e.z),
        c + float3( e.x,  e.y,  e.z)
    };

    float minNdcX =  1e9f, minNdcY =  1e9f, minNdcZ =  1e9f;
    float maxNdcX = -1e9f, maxNdcY = -1e9f;
    bool anyBehind = false;

    [unroll]
    for (int i = 0; i < 8; i++)
    {
        float4 clip = mul(float4(corners[i], 1.0f), viewProj);

        if (clip.w <= 0.0001f)
        {
            // Corner behind near plane — treat whole object as visible (conservative)
            anyBehind = true;
            break;
        }

        float3 ndc = clip.xyz / clip.w;
        minNdcX = min(minNdcX, ndc.x);
        maxNdcX = max(maxNdcX, ndc.x);
        minNdcY = min(minNdcY, ndc.y);
        maxNdcY = max(maxNdcY, ndc.y);
        minNdcZ = min(minNdcZ, ndc.z);   // nearest NDC depth (smallest z = closest)
    }

    if (anyBehind)
    {
        g_results[idx] = 0u;  // visible
        return;
    }

    // Convert NDC [-1,+1] to UV [0,1]
    // NDC.y=+1 → UV.v=0 (top), NDC.y=-1 → UV.v=1 (bottom)
    float uvMinX = clamp(minNdcX * 0.5f + 0.5f, 0.0f, 1.0f);
    float uvMaxX = clamp(maxNdcX * 0.5f + 0.5f, 0.0f, 1.0f);
    float uvMinY = clamp(1.0f - (maxNdcY * 0.5f + 0.5f), 0.0f, 1.0f);
    float uvMaxY = clamp(1.0f - (minNdcY * 0.5f + 0.5f), 0.0f, 1.0f);

    // Screen-space extent of the AABB in pixels
    float pixW = (uvMaxX - uvMinX) * (float)screenWidth;
    float pixH = (uvMaxY - uvMinY) * (float)screenHeight;
    float maxExtent = max(pixW, pixH);

    // Select Hi-Z mip: mip = floor(log2(maxExtent)), clamped to valid range
    uint mipLevels;
    {
        uint dummyW, dummyH;
        g_hiz.GetDimensions(0u, dummyW, dummyH, mipLevels);
    }
    int mipLevel = 0;
    if (maxExtent > 1.0f)
        mipLevel = (int)clamp(floor(log2(maxExtent)), 0.0f, (float)(mipLevels - 1));

    // Sample Hi-Z at the chosen mip level (load at AABB screen-space center)
    uint hizW, hizH, dummy;
    g_hiz.GetDimensions((uint)mipLevel, hizW, hizH, dummy);

    float sampleU = (uvMinX + uvMaxX) * 0.5f;
    float sampleV = (uvMinY + uvMaxY) * 0.5f;
    uint2 sampleXY = uint2(
        (uint)clamp(sampleU * (float)hizW, 0.0f, (float)(hizW - 1)),
        (uint)clamp(sampleV * (float)hizH, 0.0f, (float)(hizH - 1))
    );

    float hiZDepth = g_hiz.Load(int3(sampleXY, mipLevel));

    // Clamp nearest NDC depth to [0,1] (D3D12: near=0, far=1)
    float nearDepth = clamp(minNdcZ, 0.0f, 1.0f);

    // Occluded if the nearest point of the AABB is farther than the Hi-Z MAX occluder
    // nearDepth > hiZDepth → AABB is behind all geometry in this region
    g_results[idx] = (nearDepth > hiZDepth) ? 1u : 0u;
}
