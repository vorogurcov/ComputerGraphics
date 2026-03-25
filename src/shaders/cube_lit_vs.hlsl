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

struct VS_INPUT {
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    float3 Tangent : TANGENT;
};

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 UV : TEXCOORD2;
    float3 WorldTangent : TEXCOORD3;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    float4 worldPos = mul(model, float4(input.Pos, 1.0f));
    output.Pos = mul(vp, worldPos);
    output.WorldPos = worldPos.xyz;
    output.WorldNormal = normalize(mul((float3x3)normalMatrix, input.Normal));
    output.UV = input.UV;
    output.WorldTangent = normalize(mul((float3x3)normalMatrix, input.Tangent));
    return output;
}
