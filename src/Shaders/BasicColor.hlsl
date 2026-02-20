// BasicColor.hlsl - Vertex/Pixel shader for colored geometry with per-pixel lighting

cbuffer PerObjectCB : register(b0)
{
    float4x4 World;
    float4x4 ViewProj;
    float3 LightPosition;
    float _pad1;
    float3 LightColor;
    float _pad2;
    float3 CameraPosition;
    float _pad3;
    float3 AmbientColor;
    float _pad4;
    float Kc;
    float Kl;
    float Kq;
    float _pad5;
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
    output.position = mul(worldPos, ViewProj);
    output.normal = normalize(mul(input.normal, (float3x3)World));
    output.color = input.color;

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(LightPosition - input.worldPos);

    // Distance attenuation
    float d = length(LightPosition - input.worldPos);
    float attenuation = 1.0f / (Kc + Kl * d + Kq * d * d);

    // Diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = diff * LightColor * attenuation;

    // Final color: (ambient + diffuse) * face color
    float3 result = (AmbientColor + diffuse) * input.color.rgb;
    return float4(result, input.color.a);
}
