// ShadowDepth.hlsl - Depth-only shader for shadow map generation
// World matrix is supplied per-instance via Input Layout slot 1 (instanced rendering).

cbuffer ShadowPassCB : register(b0)
{
    float4x4 LightViewProj;
    // World is no longer in CB — comes from per-instance vertex stream
};

struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;
    float4 tangent  : TANGENT;
    // Per-instance world matrix rows (slot 1)
    float4 instWorld0 : INSTANCE_WORLD0;
    float4 instWorld1 : INSTANCE_WORLD1;
    float4 instWorld2 : INSTANCE_WORLD2;
    float4 instWorld3 : INSTANCE_WORLD3;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float4x4 World = float4x4(input.instWorld0, input.instWorld1,
                               input.instWorld2, input.instWorld3);
    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.position = mul(worldPos, LightViewProj);
    return output;
}
