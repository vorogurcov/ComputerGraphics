#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <assert.h>

#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = nullptr; }

struct RenderDevice {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* backBufferRTV = nullptr;
    D3D11_VIEWPORT viewport = {};

    UINT width = 1280;
    UINT height = 720;

    void Init(HWND hWnd);
    void CreateResources(UINT w, UINT h);
    void Resize(UINT newWidth, UINT newHeight);
    void PrepareFrame(const FLOAT clearColor[4]);
    void EndFrame();
    void Cleanup();
};