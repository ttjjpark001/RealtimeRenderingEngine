// GPU Occlusion Test Compute Shader (Phase 32)
// Tests each AABB against the Hi-Z buffer, writing 0=visible or 1=occluded per node.
// 1-frame latency: results dispatched in frame N are read by CPU in frame N+1.
// Uses conservative MAX-filter Hi-Z: an object is culled only if its nearest NDC depth
// exceeds the Hi-Z MAX across all 4 corners of its screen footprint.

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

    // Footprint outside screen — treat as visible (conservative)
    if (uvMinX >= 1.0f || uvMaxX <= 0.0f || uvMinY >= 1.0f || uvMaxY <= 0.0f)
    {
        g_results[idx] = 0u;
        return;
    }

    // Screen-space extent of the AABB in pixels
    float pixW = (uvMaxX - uvMinX) * (float)screenWidth;
    float pixH = (uvMaxY - uvMinY) * (float)screenHeight;
    float maxExtent = max(pixW, pixH);

    // Select Hi-Z mip: mip = floor(log2(maxExtent)), clamped to valid range.
    // At this mip each texel covers ~maxExtent pixels, so the footprint spans ~1-2 texels.
    uint mipLevels;
    {
        uint dummyW, dummyH;
        g_hiz.GetDimensions(0u, dummyW, dummyH, mipLevels);
    }
    int mipLevel = 0;
    if (maxExtent > 1.0f)
        mipLevel = (int)clamp(floor(log2(maxExtent)), 0.0f, (float)(mipLevels - 1));

    // Get Hi-Z dimensions at the chosen mip
    uint hizW, hizH, dummy;
    g_hiz.GetDimensions((uint)mipLevel, hizW, hizH, dummy);

    float hizWf = (float)hizW;
    float hizHf = (float)hizH;

    // Sample Hi-Z at all 4 corners of the AABB screen footprint and take MAX.
    // Using MAX of 4 samples ensures: if any part of the footprint is open (far/background),
    // the test correctly says NOT occluded (conservative, avoids false positives).
    float2 uvCorners[4] = {
        float2(uvMinX, uvMinY),
        float2(uvMaxX, uvMinY),
        float2(uvMinX, uvMaxY),
        float2(uvMaxX, uvMaxY)
    };

    float maxHiZ = 0.0f;
    [unroll]
    for (int j = 0; j < 4; j++)
    {
        uint2 xy = uint2(
            (uint)clamp(uvCorners[j].x * hizWf, 0.0f, hizWf - 1.0f),
            (uint)clamp(uvCorners[j].y * hizHf, 0.0f, hizHf - 1.0f)
        );
        maxHiZ = max(maxHiZ, g_hiz.Load(int3(xy, mipLevel)));
    }

    // Clamp nearest NDC depth to [0,1] (D3D12: near=0, far=1)
    float nearDepth = clamp(minNdcZ, 0.0f, 1.0f);

    // Occluded only if the AABB nearest point is farther than the MAX Hi-Z across all 4 corners.
    // This ensures the entire footprint is covered by closer geometry before culling.
    g_results[idx] = (nearDepth > maxHiZ) ? 1u : 0u;
}
