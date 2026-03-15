// Wireframe.hlsl - Simple wireframe rendering (no lighting, no textures)

cbuffer PerObjectCB : register(b0)
{
    float4x4 World;
    float4x4 ViewProj;
    float3 CameraPosition;
    float _padObj;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;
    float4 tangent  : TANGENT;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.position = mul(worldPos, ViewProj);
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(0.8f, 0.8f, 0.8f, 1.0f);
}
