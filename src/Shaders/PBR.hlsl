// PBR.hlsl - Cook-Torrance BRDF with multi-light support and Normal Mapping

static const float PI = 3.14159265359f;
static const uint MAX_LIGHTS = 8;

// ---------------------------------------------------------------------------
// Constant Buffers
// ---------------------------------------------------------------------------
cbuffer PerObjectCB : register(b0)
{
    float4x4 World;
    float4x4 ViewProj;
    float3 CameraPosition;
    float _padObj;
};

struct LightData
{
    float3 position;
    float intensity;
    float3 color;
    float _pad0;
    float Kc;
    float Kl;
    float Kq;
    float _pad1;
};

cbuffer LightsCB : register(b1)
{
    LightData lights[MAX_LIGHTS];
    uint numActiveLights;
    float3 _padLights;
};

cbuffer PerMaterialCB : register(b2)
{
    float4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    uint hasAlbedoMap;
    uint hasNormalMap;
    uint hasMetallicRoughnessMap;
    uint hasEmissiveMap;
    uint hasOcclusionMap;
    float3 emissiveFactor;
    float _padMat;
};

// ---------------------------------------------------------------------------
// Textures & Sampler
// ---------------------------------------------------------------------------
Texture2D AlbedoMap            : register(t0);
Texture2D NormalMap            : register(t1);
Texture2D MetallicRoughnessMap : register(t2);
Texture2D EmissiveMap          : register(t3);
Texture2D OcclusionMap         : register(t4);

SamplerState LinearSampler : register(s0);

// ---------------------------------------------------------------------------
// Vertex / Pixel structures
// ---------------------------------------------------------------------------
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
    float2 texCoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float3 T        : TEXCOORD2;
    float3 B        : TEXCOORD3;
    float3 N        : TEXCOORD4;
};

// ---------------------------------------------------------------------------
// Vertex Shader
// ---------------------------------------------------------------------------
PSInput VSMain(VSInput input)
{
    PSInput output;

    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.worldPos = worldPos.xyz;
    output.position = mul(worldPos, ViewProj);
    output.texCoord = input.texCoord;

    // Build TBN matrix in world space
    float3 N = normalize(mul(input.normal, (float3x3)World));
    float3 T = normalize(mul(input.tangent.xyz, (float3x3)World));
    // Re-orthogonalize T with respect to N (Gram-Schmidt)
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T) * input.tangent.w;

    output.T = T;
    output.B = B;
    output.N = N;

    return output;
}

// ---------------------------------------------------------------------------
// BRDF Helper Functions
// ---------------------------------------------------------------------------

// GGX/Trowbridge-Reitz Normal Distribution Function
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * denom * denom);
}

// Smith-Schlick GGX Geometry Function (single direction)
float GeometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotX / (NdotX * (1.0f - k) + k);
}

// Smith's method for combined geometry obstruction
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Schlick Fresnel Approximation
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// ---------------------------------------------------------------------------
// Pixel Shader
// ---------------------------------------------------------------------------
float4 PSMain(PSInput input) : SV_TARGET
{
    // --- Sample textures or use factor fallback ---

    // Albedo
    float4 albedo4;
    if (hasAlbedoMap)
        albedo4 = AlbedoMap.Sample(LinearSampler, input.texCoord) * baseColorFactor;
    else
        albedo4 = baseColorFactor;
    float3 albedo = albedo4.rgb;
    float alpha = albedo4.a;

    // Metallic / Roughness (glTF packing: G=roughness, B=metallic)
    float metallic = metallicFactor;
    float roughness = roughnessFactor;
    if (hasMetallicRoughnessMap)
    {
        float4 mr = MetallicRoughnessMap.Sample(LinearSampler, input.texCoord);
        roughness *= mr.g;
        metallic *= mr.b;
    }
    roughness = clamp(roughness, 0.04f, 1.0f);

    // Normal
    float3 N;
    if (hasNormalMap)
    {
        float3 normalSample = NormalMap.Sample(LinearSampler, input.texCoord).rgb;
        normalSample = normalSample * 2.0f - 1.0f;
        float3x3 TBN = float3x3(normalize(input.T), normalize(input.B), normalize(input.N));
        N = normalize(mul(normalSample, TBN));
    }
    else
    {
        N = normalize(input.N);
    }

    // Emissive
    float3 emissive = emissiveFactor;
    if (hasEmissiveMap)
        emissive *= EmissiveMap.Sample(LinearSampler, input.texCoord).rgb;

    // Occlusion
    float ao = 1.0f;
    if (hasOcclusionMap)
        ao = OcclusionMap.Sample(LinearSampler, input.texCoord).r;

    // --- Lighting ---
    float3 V = normalize(CameraPosition - input.worldPos);
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float3 Lo = float3(0.0f, 0.0f, 0.0f);

    for (uint i = 0; i < numActiveLights; i++)
    {
        float3 lightPos = lights[i].position;
        float3 lightColor = lights[i].color * lights[i].intensity;

        float3 L = normalize(lightPos - input.worldPos);
        float3 H = normalize(V + L);

        // Distance attenuation
        float d = length(lightPos - input.worldPos);
        float attenuation = 1.0f / (lights[i].Kc + lights[i].Kl * d + lights[i].Kq * d * d);

        float NdotL = max(dot(N, L), 0.0f);

        // Cook-Torrance BRDF
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);

        float NdotV = max(dot(N, V), 0.0001f);
        float3 specular = (D * G * F) / (4.0f * NdotV * NdotL + 0.0001f);

        // Diffuse (energy conservation)
        float3 kD = (1.0f - F) * (1.0f - metallic);
        float3 diffuse = kD * albedo / PI;

        Lo += (diffuse + specular) * lightColor * attenuation * NdotL;
    }

    // Ambient (simple constant)
    float3 ambient = float3(0.03f, 0.03f, 0.03f) * albedo * ao;

    float3 color = ambient + Lo + emissive;

    // HDR tone mapping (Reinhard)
    color = color / (color + 1.0f);

    // Gamma correction (Method B: manual pow)
    color = pow(max(color, 0.0f), 1.0f / 2.2f);

    return float4(color, alpha);
}
