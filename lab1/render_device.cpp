#include "RENDER_DEVICE.H"

void RenderDevice::Init(HWND hWnd) {
    IDXGIFactory* factory = nullptr;
    CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);

    IDXGIAdapter* adapter = nullptr;
    factory->EnumAdapters(0, &adapter);

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL level;
    D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &device, &level, &context);
    SAFE_RELEASE(adapter);

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    factory->CreateSwapChain(device, &scd, &swapChain);
    SAFE_RELEASE(factory);

    CreateResources(width, height);
}

void RenderDevice::CreateResources(UINT w, UINT h) {
    width = w; height = h;
    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    device->CreateRenderTargetView(backBuffer, nullptr, &backBufferRTV);
    SAFE_RELEASE(backBuffer);

    viewport.Width = (FLOAT)w;
    viewport.Height = (FLOAT)h;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
}

void RenderDevice::Resize(UINT nw, UINT nh) {
    if (nw == 0 || nh == 0) return;
    context->OMSetRenderTargets(0, nullptr, nullptr);
    SAFE_RELEASE(backBufferRTV);
    swapChain->ResizeBuffers(0, nw, nh, DXGI_FORMAT_UNKNOWN, 0);
    CreateResources(nw, nh);
}

void RenderDevice::PrepareFrame(const FLOAT clearColor[4]) {
    context->RSSetViewports(1, &viewport);
    context->OMSetRenderTargets(1, &backBufferRTV, nullptr);
    context->ClearRenderTargetView(backBufferRTV, clearColor);
}

void RenderDevice::EndFrame() {
    swapChain->Present(1, 0);
}

void RenderDevice::Cleanup() {
    SAFE_RELEASE(backBufferRTV);
    SAFE_RELEASE(swapChain);
    SAFE_RELEASE(context);
    SAFE_RELEASE(device);
}