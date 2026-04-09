#include "lighting_common.hlsli"

struct GeomData {
    float4x4 model;
    float4x4 normalMatrix;
    float4 shineSpeedTexIdNM;
};

cbuffer GeomBufferInst : register(b0) {
    GeomData geomBuffer[100];
};

cbuffer SceneBuffer : register(b1) {
    float4x4 vp;
    float4 cameraPos;
    float4 ambientColor;
    uint lightCount;
    float3 _scenePad;
    PointLight lights[MAX_POINT_LIGHTS];
};

StructuredBuffer<uint> visibleIds : register(t2);

struct VS_INPUT {
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    float3 Tangent : TANGENT;
    uint InstanceId : SV_InstanceID;
};

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 UV : TEXCOORD2;
    float3 WorldTangent : TEXCOORD3;
    nointerpolation uint InstanceId : TEXCOORD4;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    uint realIdx = visibleIds[input.InstanceId];
    float4 worldPos = mul(geomBuffer[realIdx].model, float4(input.Pos, 1.0f));
    output.Pos = mul(vp, worldPos);
    output.WorldPos = worldPos.xyz;
    output.WorldNormal = normalize(mul((float3x3)geomBuffer[realIdx].normalMatrix, input.Normal));
    output.UV = input.UV;
    output.WorldTangent = normalize(mul((float3x3)geomBuffer[realIdx].normalMatrix, input.Tangent));
    output.InstanceId = input.InstanceId;
    return output;
}
