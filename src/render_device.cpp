#include "RENDER_DEVICE.H"
#include <d3dcompiler.h>
#include <windows.h>
#include <string>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

namespace {

std::wstring DirectoryOfW(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"/\\");
    if (slash == std::wstring::npos) return L".";
    return path.substr(0, slash);
}

std::wstring JoinPathW(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    const wchar_t back = a.back();
    if (back == L'\\' || back == L'/') return a + b;
    return a + L"\\" + b;
}

std::wstring GetExecutableDirectoryW() {
    wchar_t path[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return L".";
    return DirectoryOfW(std::wstring(path, path + len));
}

bool FileExistsW(const std::wstring& path) {
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool CompileShaderFromCandidates(
    const std::vector<std::wstring>& candidates,
    const char* entryPoint,
    const char* target,
    ID3DBlob** outBlob) {
    if (!outBlob) return false;
    *outBlob = nullptr;

    for (const std::wstring& candidate : candidates) {
        if (!FileExistsW(candidate)) continue;
        ID3DBlob* errors = nullptr;
        HRESULT hr = D3DCompileFromFile(
            candidate.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint,
            target,
            0,
            0,
            outBlob,
            &errors);
        if (SUCCEEDED(hr)) {
            SAFE_RELEASE(errors);
            return true;
        }
        if (errors) {
            OutputDebugStringA((const char*)errors->GetBufferPointer());
        }
        SAFE_RELEASE(errors);
    }

    return false;
}

}

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

    const std::wstring exeDir = GetExecutableDirectoryW();
    const std::vector<std::wstring> vsCandidates = {
        JoinPathW(exeDir, L"shaders\\post_process_vs.hlsl"),
        JoinPathW(exeDir, L"src\\shaders\\post_process_vs.hlsl"),
        L"shaders\\post_process_vs.hlsl",
        L"src\\shaders\\post_process_vs.hlsl"
    };
    const std::vector<std::wstring> psCandidates = {
        JoinPathW(exeDir, L"shaders\\post_process_ps.hlsl"),
        JoinPathW(exeDir, L"src\\shaders\\post_process_ps.hlsl"),
        L"shaders\\post_process_ps.hlsl",
        L"src\\shaders\\post_process_ps.hlsl"
    };
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    const bool vsOk = CompileShaderFromCandidates(vsCandidates, "main", "vs_4_0", &vsBlob);
    const bool psOk = CompileShaderFromCandidates(psCandidates, "main", "ps_4_0", &psBlob);
    if (!vsOk || !psOk || !vsBlob || !psBlob) {
        OutputDebugStringA("Post-process shader compile failed.\n");
        SAFE_RELEASE(vsBlob);
        SAFE_RELEASE(psBlob);
        return;
    }

    HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &postProcessVS);
    if (FAILED(hr)) {
        SAFE_RELEASE(vsBlob);
        SAFE_RELEASE(psBlob);
        return;
    }
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &postProcessPS);
    if (FAILED(hr)) {
        SAFE_RELEASE(vsBlob);
        SAFE_RELEASE(psBlob);
        return;
    }
    SAFE_RELEASE(vsBlob);
    SAFE_RELEASE(psBlob);

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0.0f;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device->CreateSamplerState(&sampDesc, &postProcessSampler);
    assert(SUCCEEDED(hr));

    CreateResources(width, height);
}

void RenderDevice::CreateResources(UINT w, UINT h) {
    width = w; height = h;
    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    device->CreateRenderTargetView(backBuffer, nullptr, &backBufferRTV);
    SAFE_RELEASE(backBuffer);

    D3D11_TEXTURE2D_DESC sceneDesc = {};
    sceneDesc.Width = w;
    sceneDesc.Height = h;
    sceneDesc.MipLevels = 1;
    sceneDesc.ArraySize = 1;
    sceneDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sceneDesc.SampleDesc.Count = 1;
    sceneDesc.Usage = D3D11_USAGE_DEFAULT;
    sceneDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    device->CreateTexture2D(&sceneDesc, nullptr, &sceneColorTexture);
    device->CreateRenderTargetView(sceneColorTexture, nullptr, &sceneColorRTV);
    device->CreateShaderResourceView(sceneColorTexture, nullptr, &sceneColorSRV);

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = w;
    depthDesc.Height = h;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    device->CreateTexture2D(&depthDesc, nullptr, &depthStencilBuffer);
    device->CreateDepthStencilView(depthStencilBuffer, nullptr, &depthStencilView);

    if (!opaqueDepthState) {
        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsd.DepthFunc = D3D11_COMPARISON_GREATER;
        dsd.StencilEnable = FALSE;
        device->CreateDepthStencilState(&dsd, &opaqueDepthState);
    }
    if (!transparentDepthState) {
        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc = D3D11_COMPARISON_GREATER;
        dsd.StencilEnable = FALSE;
        device->CreateDepthStencilState(&dsd, &transparentDepthState);
    }
    if (!blendStateAlpha) {
        D3D11_BLEND_DESC bd = {};
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&bd, &blendStateAlpha);
    }

    viewport.Width = (FLOAT)w;
    viewport.Height = (FLOAT)h;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
}

void RenderDevice::Resize(UINT nw, UINT nh) {
    if (nw == 0 || nh == 0) return;
    context->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->PSSetShaderResources(0, 1, &nullSRV);
    SAFE_RELEASE(backBufferRTV);
    SAFE_RELEASE(sceneColorSRV);
    SAFE_RELEASE(sceneColorRTV);
    SAFE_RELEASE(sceneColorTexture);
    SAFE_RELEASE(depthStencilView);
    SAFE_RELEASE(depthStencilBuffer);
    swapChain->ResizeBuffers(0, nw, nh, DXGI_FORMAT_UNKNOWN, 0);
    CreateResources(nw, nh);
}

void RenderDevice::PrepareFrame(const FLOAT clearColor[4]) {
    context->RSSetViewports(1, &viewport);
    context->OMSetRenderTargets(1, &sceneColorRTV, depthStencilView);
    context->ClearRenderTargetView(sceneColorRTV, clearColor);
    context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 0.0f, 0);
    context->OMSetDepthStencilState(opaqueDepthState, 0);
}

void RenderDevice::EndFrame() {
    if (!postProcessVS || !postProcessPS || !postProcessSampler || !sceneColorSRV) {
        swapChain->Present(1, 0);
        return;
    }

    context->OMSetRenderTargets(1, &backBufferRTV, nullptr);
    context->OMSetDepthStencilState(nullptr, 0);
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11Buffer* nullVB = nullptr;
    UINT zero = 0;
    context->IASetVertexBuffers(0, 1, &nullVB, &zero, &zero);
    context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);

    context->VSSetShader(postProcessVS, nullptr, 0);
    context->PSSetShader(postProcessPS, nullptr, 0);
    context->PSSetSamplers(0, 1, &postProcessSampler);
    context->PSSetShaderResources(0, 1, &sceneColorSRV);
    context->Draw(3, 0);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->PSSetShaderResources(0, 1, &nullSRV);

    swapChain->Present(1, 0);
}

void RenderDevice::Cleanup() {
    SAFE_RELEASE(postProcessSampler);
    SAFE_RELEASE(postProcessPS);
    SAFE_RELEASE(postProcessVS);
    SAFE_RELEASE(blendStateAlpha);
    SAFE_RELEASE(transparentDepthState);
    SAFE_RELEASE(opaqueDepthState);
    SAFE_RELEASE(depthStencilView);
    SAFE_RELEASE(depthStencilBuffer);
    SAFE_RELEASE(sceneColorSRV);
    SAFE_RELEASE(sceneColorRTV);
    SAFE_RELEASE(sceneColorTexture);
    SAFE_RELEASE(backBufferRTV);
    SAFE_RELEASE(swapChain);
    SAFE_RELEASE(context);
    SAFE_RELEASE(device);
}