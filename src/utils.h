#pragma once

#include <DirectXMath.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = nullptr; }
#endif

inline DirectX::XMMATRIX XMMatrixPerspectiveFovLHReversed(float FovAngleY, float AspectRatio, float NearZ, float FarZ) {
    float h = 1.0f / tanf(FovAngleY * 0.5f);
    float w = h / AspectRatio;
    float n = NearZ, f = FarZ;
    float A = -n / (f - n);
    float B = n * f / (f - n);
    return DirectX::XMMATRIX(
        w, 0, 0, 0,
        0, h, 0, 0,
        0, 0, A, B,
        0, 0, 1, 0
    );
}