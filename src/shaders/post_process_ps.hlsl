Texture2D sceneTexture : register(t0);
SamplerState sceneSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float3 color = sceneTexture.Sample(sceneSampler, uv).rgb;

    // Sepia filter.
    float3 sepia;
    sepia.r = dot(color, float3(0.393f, 0.769f, 0.189f));
    sepia.g = dot(color, float3(0.349f, 0.686f, 0.168f));
    sepia.b = dot(color, float3(0.272f, 0.534f, 0.131f));
    sepia = saturate(sepia);

    return float4(sepia, 1.0f);
}
