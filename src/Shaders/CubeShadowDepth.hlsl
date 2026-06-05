// CubeShadowDepth.hlsl - Point light cube shadow map depth pass
// Outputs linear depth (distance / farPlane) as R32_FLOAT color.

cbuffer CubeShadowPassCB : register(b0)
{
    float4x4 LightViewProj;
    float3   LightPos;
    float    FarPlane;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;
    float4 tangent  : TANGENT;
    float4 instWorld0 : INSTANCE_WORLD0;
    float4 instWorld1 : INSTANCE_WORLD1;
    float4 instWorld2 : INSTANCE_WORLD2;
    float4 instWorld3 : INSTANCE_WORLD3;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float4x4 World = float4x4(input.instWorld0, input.instWorld1,
                               input.instWorld2, input.instWorld3);
    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.worldPos  = worldPos.xyz;
    output.position  = mul(worldPos, LightViewProj);
    return output;
}

float PSMain(VSOutput input) : SV_TARGET
{
    return length(input.worldPos - LightPos) / FarPlane;
}
