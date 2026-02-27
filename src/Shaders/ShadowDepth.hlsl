// ShadowDepth.hlsl - Depth-only shader for shadow map generation

cbuffer ShadowPassCB : register(b0)
{
    float4x4 LightViewProj;
    float4x4 World;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;
    float4 tangent  : TANGENT;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.position = mul(worldPos, LightViewProj);
    return output;
}
