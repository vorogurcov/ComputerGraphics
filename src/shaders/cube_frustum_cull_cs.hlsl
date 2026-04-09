#define MAX_CUBE_INSTANCES 100

cbuffer CullBuffer : register(b0) {
    float4 frustumPlanes[6];
    uint objectCount;
    float3 _pad0;
    float4 bbMin[MAX_CUBE_INSTANCES];
    float4 bbMax[MAX_CUBE_INSTANCES];
};

RWStructuredBuffer<uint> drawArgs : register(u0);
RWStructuredBuffer<uint> visibleIds : register(u1);

bool IsBoxInside(float3 bMin, float3 bMax) {
    [unroll]
    for (int i = 0; i < 6; ++i) {
        float3 p = float3(
            frustumPlanes[i].x >= 0.0f ? bMax.x : bMin.x,
            frustumPlanes[i].y >= 0.0f ? bMax.y : bMin.y,
            frustumPlanes[i].z >= 0.0f ? bMax.z : bMin.z);
        if (dot(frustumPlanes[i].xyz, p) + frustumPlanes[i].w < 0.0f) {
            return false;
        }
    }
    return true;
}

[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    const uint id = dtid.x;
    if (id >= objectCount) {
        return;
    }

    if (!IsBoxInside(bbMin[id].xyz, bbMax[id].xyz)) {
        return;
    }

    uint outIndex = 0;
    InterlockedAdd(drawArgs[1], 1, outIndex);
    visibleIds[outIndex] = id;
}
