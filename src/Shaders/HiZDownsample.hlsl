// Hi-Z Downsampling Compute Shader (Phase 32)
// Generates the Hierarchical-Z mip chain using a max-filter for conservative occlusion culling.
// - isFirstMip = 1: 1:1 copy from depth buffer SRV to Hi-Z UAV mip 0
// - isFirstMip = 0: 2x2 max-filter downsample from Hi-Z SRV mip N to UAV mip N+1

cbuffer HiZConstants : register(b0)
{
    uint srcWidth;
    uint srcHeight;
    uint dstWidth;
    uint dstHeight;
    uint isFirstMip;
    uint pad0;
    uint pad1;
    uint pad2;
};

Texture2D<float>   g_src : register(t0);
RWTexture2D<float> g_dst : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= dstWidth || tid.y >= dstHeight)
        return;

    if (isFirstMip)
    {
        // Mip 0: 1:1 copy from depth buffer (same resolution)
        uint2 src = min(tid.xy, uint2(srcWidth - 1, srcHeight - 1));
        g_dst[tid.xy] = g_src[src];
    }
    else
    {
        // Mip N+1: 2x2 max-filter from mip N
        // MAX filter is conservative: if AABB nearest depth > Hi-Z MAX, it is definitely occluded
        uint2 src = tid.xy * 2u;
        float d0 = g_src[uint2(min(src.x,     srcWidth - 1), min(src.y,     srcHeight - 1))];
        float d1 = g_src[uint2(min(src.x + 1, srcWidth - 1), min(src.y,     srcHeight - 1))];
        float d2 = g_src[uint2(min(src.x,     srcWidth - 1), min(src.y + 1, srcHeight - 1))];
        float d3 = g_src[uint2(min(src.x + 1, srcWidth - 1), min(src.y + 1, srcHeight - 1))];
        g_dst[tid.xy] = max(max(d0, d1), max(d2, d3));
    }
}
