#include "lighting_common.hlsli"

cbuffer ModelBuffer : register(b0) {
    float4x4 model;
    float4x4 normalMatrix;
};

cbuffer SceneBuffer : register(b1) {
    float4x4 vp;
    float4 cameraPos;
    float4 ambientColor;
    uint lightCount;
    float3 _scenePad;
    PointLight lights[MAX_POINT_LIGHTS];
};

Texture2D colorTexture : register(t0);
Texture2D normalTexture : register(t1);
SamplerState colorSampler : register(s0);

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 UV : TEXCOORD2;
    float3 WorldTangent : TEXCOORD3;
};

float4 main(PS_INPUT input) : SV_TARGET {
    float3 albedo = colorTexture.Sample(colorSampler, input.UV).rgb;
    float3 worldNormal = normalize(input.WorldNormal);

#if USE_NORMAL_MAP
    float3 tangent = normalize(input.WorldTangent);
    float3 binormal = normalize(cross(worldNormal, tangent));
    float3x3 tbn = float3x3(tangent, binormal, worldNormal);
    float3 sampledNormal = normalTexture.Sample(colorSampler, input.UV).xyz * 2.0f - 1.0f;
    worldNormal = normalize(mul(tbn, sampledNormal));
#endif

    float3 viewDir = normalize(cameraPos.xyz - input.WorldPos);
    float3 color = albedo * ambientColor.rgb;
    uint count = min(lightCount, (uint)MAX_POINT_LIGHTS);

    [loop]
    for (uint i = 0; i < count; ++i) {
        color += ComputePhongPointLight(albedo, input.WorldPos, worldNormal, viewDir, lights[i], 32.0f);
    }

    return float4(saturate(color), 1.0f);
}
