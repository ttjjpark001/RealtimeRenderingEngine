// PBR.hlsl - Cook-Torrance BRDF with multi-light support and Normal Mapping

static const float PI = 3.14159265359f;
static const uint MAX_LIGHTS = 16;

// ---------------------------------------------------------------------------
// Constant Buffers
// ---------------------------------------------------------------------------
// World matrix is supplied per-instance via the vertex input stream (slot 1).
// CB0 carries only the view-projection and camera data (shared across all instances in a batch).
cbuffer PerObjectCB : register(b0)
{
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
    uint type;            // 0=Directional, 1=Point, 2=Spot
    float3 direction;
    float innerConeAngle;
    float outerConeAngle;
    int shadowMapIndex;   // -1 = no shadow, 0~7 = shadow map index
    float2 _pad1;         // NOTE: must be float2, NOT float[2] — HLSL array packing pads each element to 16 bytes
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
    uint alphaMode;       // 0=Opaque, 1=Mask, 2=Blend
    uint useMips;         // 1=use mip chain, 0=force mip 0 (MipMap toggle)
    uint3 _padMat;        // padding
};

static const uint MAX_SHADOW_MAPS  = 8;
static const uint CSM_NUM_CASCADES = 3;

cbuffer ShadowCB : register(b3)
{
    float4x4 lightViewProj[MAX_SHADOW_MAPS];
    uint  shadowMapCount;
    float shadowTexelSize;          // 1.0 / shadowMapResolution
    float shadowNormalBiasWorld;    // world-space normal offset = orthoSize/mapSize * 2
    uint  csmEnabled;               // 1 = CSM active for Directional lights
    float3 cascadeSplitDepths;      // view-space Z far boundary per cascade (x=c0, y=c1, z=c2)
    uint  csmDebugView;             // 1 = tint by cascade index (red/green/blue)
    float3 cameraForward;           // world-space camera forward direction
    float _padFwd;
};

// ---------------------------------------------------------------------------
// Textures & Samplers
// ---------------------------------------------------------------------------
Texture2D AlbedoMap            : register(t0);
Texture2D NormalMap            : register(t1);
Texture2D MetallicRoughnessMap : register(t2);
Texture2D EmissiveMap          : register(t3);
Texture2D OcclusionMap         : register(t4);

// Shadow maps (t5~t12)
Texture2D ShadowMap0 : register(t5);
Texture2D ShadowMap1 : register(t6);
Texture2D ShadowMap2 : register(t7);
Texture2D ShadowMap3 : register(t8);
Texture2D ShadowMap4 : register(t9);
Texture2D ShadowMap5 : register(t10);
Texture2D ShadowMap6 : register(t11);
Texture2D ShadowMap7 : register(t12);

SamplerState LinearSampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

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
    // Per-instance world matrix (4 rows), bound in Input Layout slot 1
    float4 instWorld0 : INSTANCE_WORLD0;
    float4 instWorld1 : INSTANCE_WORLD1;
    float4 instWorld2 : INSTANCE_WORLD2;
    float4 instWorld3 : INSTANCE_WORLD3;
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

    // Reconstruct world matrix from per-instance rows
    float4x4 World = float4x4(input.instWorld0, input.instWorld1,
                               input.instWorld2, input.instWorld3);

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
// Shadow Mapping (PCF 3x3)
// ---------------------------------------------------------------------------
float SampleShadowMap(uint idx, float2 uv, float depth)
{
    // result is pre-initialized to the "lit" default so FXC can prove it is
    // always initialized before return (avoiding X4000 with SampleCmpLevelZero).
    float result = 1.0f;
    if      (idx == 0) result = ShadowMap0.SampleCmpLevelZero(ShadowSampler, uv, depth);
    else if (idx == 1) result = ShadowMap1.SampleCmpLevelZero(ShadowSampler, uv, depth);
    else if (idx == 2) result = ShadowMap2.SampleCmpLevelZero(ShadowSampler, uv, depth);
    else if (idx == 3) result = ShadowMap3.SampleCmpLevelZero(ShadowSampler, uv, depth);
    else if (idx == 4) result = ShadowMap4.SampleCmpLevelZero(ShadowSampler, uv, depth);
    else if (idx == 5) result = ShadowMap5.SampleCmpLevelZero(ShadowSampler, uv, depth);
    else if (idx == 6) result = ShadowMap6.SampleCmpLevelZero(ShadowSampler, uv, depth);
    else if (idx == 7) result = ShadowMap7.SampleCmpLevelZero(ShadowSampler, uv, depth);
    return result;
}

float CalcShadow(uint shadowIdx, float3 worldPos)
{
    // Pre-initialize result so FXC dataflow analysis can prove the return value
    // is always defined regardless of the SampleCmpLevelZero comparison-sampler path.
    float result = 1.0f;

    // Clamp index so FXC can prove lightViewProj access is in-bounds.
    shadowIdx = min(shadowIdx, MAX_SHADOW_MAPS - 1);
    float4 lightSpacePos = mul(float4(worldPos, 1.0f), lightViewProj[shadowIdx]);
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // NDC to UV: x[-1,1]→[0,1], y[-1,1]→[1,0] (D3D UV flip)
    float2 uv = float2(projCoords.x * 0.5f + 0.5f, -projCoords.y * 0.5f + 0.5f);
    float depth = projCoords.z;

    // Only run PCF when the fragment is inside the shadow map's NDC cube.
    // Using a conditional block instead of an early return avoids FXC X4000.
    bool inRange = (uv.x >= 0.0f && uv.x <= 1.0f &&
                    uv.y >= 0.0f && uv.y <= 1.0f &&
                    depth >= 0.0f && depth <= 1.0f);
    if (inRange)
    {
        // PCF 3x3
        float shadow = 0.0f;
        float texelSize = shadowTexelSize;
        [unroll]
        for (int y = -1; y <= 1; y++)
        {
            [unroll]
            for (int x = -1; x <= 1; x++)
            {
                shadow += saturate(SampleShadowMap(shadowIdx, uv + float2(x, y) * texelSize, depth));
            }
        }
        result = shadow / 9.0f;
    }
    return result;
}

// ---------------------------------------------------------------------------
// CSM — cascade index selection and shadow lookup
// ---------------------------------------------------------------------------
int GetCascadeIndex(float3 worldPos)
{
    // Pre-initialize so FXC can prove result is always assigned (avoids X4000).
    int result = 2;
    float viewDepth = dot(worldPos - CameraPosition, normalize(cameraForward));
    if      (viewDepth < cascadeSplitDepths.x) result = 0;
    else if (viewDepth < cascadeSplitDepths.y) result = 1;
    return result;
}

float CalcShadowCSM(uint baseIdx, float3 worldPos, float3 biasedPos)
{
    // Pre-initialize result so FXC dataflow can prove it is always assigned.
    float result    = 1.0f;
    int  cascadeIdx = GetCascadeIndex(worldPos);
    uint shadowIdx  = baseIdx + (uint)cascadeIdx;
    if (shadowIdx < shadowMapCount)
        result = CalcShadow(shadowIdx, biasedPos);
    return result;
}

// ---------------------------------------------------------------------------
// Pixel Shader
// ---------------------------------------------------------------------------
float4 PSMain(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    // --- Sample textures or use factor fallback ---

    // Helper macro: sample with mip chain or force mip 0
    // useMips==1: full mip chain (anisotropic filtering), ==0: always mip 0
    float2 uv = input.texCoord;

    // Albedo
    float4 albedo4;
    if (hasAlbedoMap)
    {
        float4 s = useMips ? AlbedoMap.Sample(LinearSampler, uv)
                           : AlbedoMap.SampleLevel(LinearSampler, uv, 0);
        albedo4 = s * baseColorFactor;
    }
    else
        albedo4 = baseColorFactor;

    float3 albedo = albedo4.rgb;
    float alpha = albedo4.a;

    // Alpha mask: discard pixel if below cutoff
    if (alphaMode == 1)
        clip(alpha - alphaCutoff);

    // Metallic / Roughness (glTF packing: G=roughness, B=metallic)
    float metallic = metallicFactor;
    float roughness = roughnessFactor;
    if (hasMetallicRoughnessMap)
    {
        float4 mr = useMips ? MetallicRoughnessMap.Sample(LinearSampler, uv)
                            : MetallicRoughnessMap.SampleLevel(LinearSampler, uv, 0);
        roughness *= mr.g;
        metallic *= mr.b;
    }
    roughness = clamp(roughness, 0.04f, 1.0f);

    // Normal
    float3 N;
    if (hasNormalMap)
    {
        float3 normalSample = (useMips ? NormalMap.Sample(LinearSampler, uv)
                                       : NormalMap.SampleLevel(LinearSampler, uv, 0)).rgb;
        normalSample = normalSample * 2.0f - 1.0f;
        float3x3 TBN = float3x3(normalize(input.T), normalize(input.B), normalize(input.N));
        N = normalize(mul(normalSample, TBN));
    }
    else
    {
        N = normalize(input.N);
    }
    // Two-sided rendering: flip normal for back faces so lighting is correct
    if (!isFrontFace)
        N = -N;

    // Emissive
    float3 emissive = emissiveFactor;
    if (hasEmissiveMap)
    {
        float3 es = (useMips ? EmissiveMap.Sample(LinearSampler, uv)
                             : EmissiveMap.SampleLevel(LinearSampler, uv, 0)).rgb;
        emissive *= es;
    }

    // Occlusion
    float ao = 1.0f;
    if (hasOcclusionMap)
    {
        ao = useMips ? OcclusionMap.Sample(LinearSampler, uv).r
                     : OcclusionMap.SampleLevel(LinearSampler, uv, 0).r;
    }

    // --- Lighting ---
    float3 V = normalize(CameraPosition - input.worldPos);
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float3 Lo = float3(0.0f, 0.0f, 0.0f);

    for (uint i = 0; i < numActiveLights; i++)
    {
        float3 lightColor = lights[i].color * lights[i].intensity;

        float3 L;
        float attenuation;

        if (lights[i].type == 0)
        {
            // Directional: no attenuation, L = -direction
            L = normalize(-lights[i].direction);
            attenuation = 1.0f;
        }
        else
        {
            // Point or Spot: distance-based attenuation
            float3 lightPos = lights[i].position;
            L = normalize(lightPos - input.worldPos);
            float d = length(lightPos - input.worldPos);
            attenuation = 1.0f / (lights[i].Kc + lights[i].Kl * d + lights[i].Kq * d * d);

            if (lights[i].type == 2)
            {
                // Spot: additional cone attenuation
                float theta = dot(-L, normalize(lights[i].direction));
                attenuation *= smoothstep(lights[i].outerConeAngle, lights[i].innerConeAngle, theta);
            }
        }

        float3 H = normalize(V + L);
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

        float shadowFactor = 1.0f;
        if (lights[i].shadowMapIndex >= 0 && (uint)lights[i].shadowMapIndex < shadowMapCount)
        {
            float3 shadowPos = input.worldPos + N * shadowNormalBiasWorld;
            if (csmEnabled && lights[i].type == 0)
                shadowFactor = CalcShadowCSM((uint)lights[i].shadowMapIndex, input.worldPos, shadowPos);
            else
                shadowFactor = CalcShadow((uint)lights[i].shadowMapIndex, shadowPos);
        }

        Lo += (diffuse + specular) * lightColor * attenuation * NdotL * shadowFactor;
    }

    // Ambient (IBL approximation)
    float3 ambient = float3(0.15f, 0.15f, 0.15f) * albedo * ao;

    float3 color = ambient + Lo + emissive;

    // CSM debug: tint pixel by cascade index (red=0, green=1, blue=2)
    if (csmEnabled && csmDebugView)
    {
        int    cascadeIdx  = GetCascadeIndex(input.worldPos);
        float3 tintColor   = float3(0.2f, 0.2f, 1.0f);  // default: blue (cascade 2)
        if      (cascadeIdx == 0) tintColor = float3(1.0f, 0.2f, 0.2f);
        else if (cascadeIdx == 1) tintColor = float3(0.2f, 1.0f, 0.2f);
        color = lerp(color, tintColor, 0.5f);
    }

    // HDR tone mapping (Reinhard)
    color = color / (color + 1.0f);

    // Gamma correction (Method B: manual pow)
    color = pow(max(color, 0.0f), 1.0f / 2.2f);

    return float4(color, alpha);
}
