// BasicColor.hlsl - Vertex/Pixel shader for colored geometry

cbuffer ObjectConstants : register(b0)
{
    float4x4 World;
    float4x4 ViewProjection;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR;
    float3 normal   : NORMAL;
};

struct PSInput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR;
    float3 normal    : NORMAL;
    float3 worldPos  : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;

    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.worldPos = worldPos.xyz;
    output.position = mul(worldPos, ViewProjection);
    output.normal = normalize(mul(input.normal, (float3x3)World));
    output.color = input.color;

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    // Pass through face color (lighting added in Phase 8)
    return input.color;
}
