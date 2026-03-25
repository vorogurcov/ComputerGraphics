#include "utils.h"
#include "SKYBOX_COMPONENT.H"
#include "dds_loader.h"

#include <assert.h>
#include <cstring>
#include <string>

namespace {
std::wstring GetExecutableDirectoryW() {
    wchar_t path[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return L".";
    std::wstring s(path, path + len);
    const size_t slash = s.find_last_of(L"/\\");
    if (slash == std::wstring::npos) return L".";
    return s.substr(0, slash);
}

std::wstring JoinPathW(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    wchar_t back = a.back();
    if (back == L'\\' || back == L'/') return a + b;
    return a + L"\\" + b;
}
}

const char* g_SkyboxVertexShaderSource = R"(
cbuffer SceneBuffer : register(b0) {
    float4x4 vp;
    float4 camPos;
};

struct VS_INPUT {
    float3 Pos : POSITION;
};

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float3 LocalDir : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;

    float3 worldPos = input.Pos + camPos.xyz;
    float4 clipPos = mul(vp, float4(worldPos, 1.0f));
    clipPos.z = 0.0f;
    output.Pos = clipPos;
    output.LocalDir = input.Pos;
    return output;
}
)";

const char* g_SkyboxPixelShaderSource = R"(
TextureCube skyTexture : register(t0);
SamplerState skySampler : register(s0);

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float3 LocalDir : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET {
    float3 dir = normalize(input.LocalDir);
    float4 c = skyTexture.Sample(skySampler, dir);
    return c * float4(0.6f, 0.7f, 1.0f, 1.0f);
}
)";

void SkyboxComponent::CompileAndCreateShaders(ID3D11Device* device) {
    HRESULT hr;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    hr = D3DCompile(g_SkyboxVertexShaderSource, strlen(g_SkyboxVertexShaderSource), "SkyboxVS", nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            SAFE_RELEASE(errorBlob);
        }
        assert(false && "Failed to compile skybox vertex shader!");
    }
    SAFE_RELEASE(errorBlob);

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
    assert(SUCCEEDED(hr));

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = device->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
    assert(SUCCEEDED(hr));
    SAFE_RELEASE(vsBlob);

    hr = D3DCompile(g_SkyboxPixelShaderSource, strlen(g_SkyboxPixelShaderSource), "SkyboxPS", nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            SAFE_RELEASE(errorBlob);
        }
        assert(false && "Failed to compile skybox pixel shader!");
    }
    SAFE_RELEASE(errorBlob);

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
    assert(SUCCEEDED(hr));
    SAFE_RELEASE(psBlob);
}

void SkyboxComponent::CreatePipelineStates(ID3D11Device* device) {
    HRESULT hr;

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
    hr = device->CreateDepthStencilState(&dsd, &depthState);
    assert(SUCCEEDED(hr));

    D3D11_RASTERIZER_DESC rsd = {};
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_NONE;
    rsd.DepthClipEnable = TRUE;
    hr = device->CreateRasterizerState(&rsd, &rasterState);
    assert(SUCCEEDED(hr));
}

void SkyboxComponent::LoadCubemapDDS(ID3D11Device* device) {
    DDSLoadedImage img;
    std::string err;
    const std::wstring exeDir = GetExecutableDirectoryW();
    const std::wstring candidates[] = {
        JoinPathW(exeDir, L"assets\\skybox.dds"),
        JoinPathW(exeDir, L"src\\assets\\skybox.dds"),
        L"assets\\skybox.dds",
        L"src\\assets\\skybox.dds",
    };

    bool ok = false;
    for (size_t i = 0; i < ARRAYSIZE(candidates); ++i) {
        if (LoadDDSFromFile(candidates[i].c_str(), img, &err)) {
            ok = true;
            std::string msg = "Skybox DDS loaded from: ";
            char buf[260];
            size_t converted = 0;
            wcstombs_s(&converted, buf, candidates[i].c_str(), _TRUNCATE);
            msg += buf;
            msg += "\n";
            OutputDebugStringA(msg.c_str());
            break;
        }
    }
    if (!ok || !img.isCubemap || img.arraySize != 6) {
        if (!ok) OutputDebugStringA(("Skybox DDS load failed: " + err + "\n").c_str());
        if (ok && !img.isCubemap) OutputDebugStringA("Skybox DDS is not a cubemap (need DDS cubemap).\n");
        if (ok && img.isCubemap && img.arraySize != 6) OutputDebugStringA("Skybox DDS: expected exactly 6 faces.\n");

        const uint32_t fallbackFaces[6] = {
            0xff4040ffu,
            0xff40ff40u,
            0xffff4040u,
            0xff40ffffu,
            0xffff40ffu,
            0xffffff40u
        };
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.ArraySize = 6;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        D3D11_SUBRESOURCE_DATA init[6] = {};
        for (int i = 0; i < 6; ++i) {
            init[i].pSysMem = &fallbackFaces[i];
            init[i].SysMemPitch = 4;
            init[i].SysMemSlicePitch = 4;
        }

        HRESULT hr = device->CreateTexture2D(&desc, init, &cubemapTexture);
        assert(SUCCEEDED(hr));

        D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
        srvd.Format = desc.Format;
        srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvd.TextureCube.MostDetailedMip = 0;
        srvd.TextureCube.MipLevels = 1;
        hr = device->CreateShaderResourceView(cubemapTexture, &srvd, &cubemapSRV);
        assert(SUCCEEDED(hr));
        return;
    }

    HRESULT hr;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = img.width;
    desc.Height = img.height;
    desc.MipLevels = img.mipCount;
    desc.ArraySize = img.arraySize;
    desc.Format = img.format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    std::vector<D3D11_SUBRESOURCE_DATA> initData;
    initData.resize((size_t)img.subresources.size());

    for (uint32_t arraySlice = 0; arraySlice < img.arraySize; ++arraySlice) {
        for (uint32_t mip = 0; mip < img.mipCount; ++mip) {
            const uint32_t sub = mip + arraySlice * img.mipCount;
            const DDSLoadedImageSubresource& sr = img.subresources[sub];

            D3D11_SUBRESOURCE_DATA& d = initData[sub];
            d.pSysMem = img.data.data() + sr.dataOffset;
            d.SysMemPitch = sr.rowPitch;
            d.SysMemSlicePitch = sr.slicePitch;
        }
    }

    hr = device->CreateTexture2D(&desc, initData.data(), &cubemapTexture);
    assert(SUCCEEDED(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = desc.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvd.TextureCube.MostDetailedMip = 0;
    srvd.TextureCube.MipLevels = desc.MipLevels;

    hr = device->CreateShaderResourceView(cubemapTexture, &srvd, &cubemapSRV);
    assert(SUCCEEDED(hr));

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_ANISOTROPIC;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxAnisotropy = 16;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MinLOD = 0.0f;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device->CreateSamplerState(&sd, &sampler);
    assert(SUCCEEDED(hr));
}

void SkyboxComponent::Init(ID3D11Device* device) {
    HRESULT hr;

    SkyboxVertex vertices[] = {
        { {-1.0f, -1.0f, -1.0f} },
        { {-1.0f,  1.0f, -1.0f} },
        { { 1.0f,  1.0f, -1.0f} },
        { { 1.0f, -1.0f, -1.0f} },
        { {-1.0f, -1.0f,  1.0f} },
        { {-1.0f,  1.0f,  1.0f} },
        { { 1.0f,  1.0f,  1.0f} },
        { { 1.0f, -1.0f,  1.0f} }
    };

    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = sizeof(vertices);
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vinit = {};
    vinit.pSysMem = vertices;
    hr = device->CreateBuffer(&vbd, &vinit, &vertexBuffer);
    assert(SUCCEEDED(hr));

    WORD indices[] = {
        0,1,2, 0,2,3,
        4,6,5, 4,7,6,
        4,5,1, 4,1,0,
        3,2,6, 3,6,7,
        1,5,6, 1,6,2,
        4,0,3, 4,3,7
    };

    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth = sizeof(indices);
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iinit = {};
    iinit.pSysMem = indices;
    hr = device->CreateBuffer(&ibd, &iinit, &indexBuffer);
    assert(SUCCEEDED(hr));

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(SkyboxSceneBuffer);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device->CreateBuffer(&cbd, nullptr, &sceneBuffer);
    assert(SUCCEEDED(hr));

    CompileAndCreateShaders(device);
    CreatePipelineStates(device);
    LoadCubemapDDS(device);
}

void SkyboxComponent::Render(ID3D11DeviceContext* context, float aspectRatio, float camPitch, float camYaw) {
    UINT stride = sizeof(SkyboxVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetInputLayout(inputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);

    context->OMSetDepthStencilState(depthState, 0);
    context->RSSetState(rasterState);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(context->Map(sceneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        SkyboxSceneBuffer* sb = (SkyboxSceneBuffer*)mapped.pData;

        DirectX::XMVECTOR cameraPos = DirectX::XMVectorSet(0.0f, 0.0f, -3.0f, 1.0f);
        DirectX::XMMATRIX cameraRot = DirectX::XMMatrixRotationRollPitchYaw(camPitch, camYaw, 0.0f);

        DirectX::XMVECTOR forward = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), cameraRot);
        DirectX::XMVECTOR up = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), cameraRot);
        DirectX::XMVECTOR focus = DirectX::XMVectorAdd(cameraPos, forward);

        DirectX::XMMATRIX v = DirectX::XMMatrixLookAtLH(cameraPos, focus, up);
        const float nearZ = 0.1f;
        const float farZ = 100.0f;
        DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PI / 3.0f, aspectRatio, farZ, nearZ);
        sb->vp = DirectX::XMMatrixMultiply(v, p);

        DirectX::XMFLOAT4 cp;
        DirectX::XMStoreFloat4(&cp, cameraPos);
        sb->camPos = cp;

        context->Unmap(sceneBuffer, 0);
    }

    context->VSSetConstantBuffers(0, 1, &sceneBuffer);
    context->PSSetShaderResources(0, 1, &cubemapSRV);
    context->PSSetSamplers(0, 1, &sampler);

    context->DrawIndexed(36, 0, 0);

    context->OMSetDepthStencilState(nullptr, 0);
    context->RSSetState(nullptr);
}

void SkyboxComponent::Cleanup() {
    SAFE_RELEASE(rasterState);
    SAFE_RELEASE(depthState);

    SAFE_RELEASE(sampler);
    SAFE_RELEASE(cubemapSRV);
    SAFE_RELEASE(cubemapTexture);

    SAFE_RELEASE(sceneBuffer);
    SAFE_RELEASE(indexBuffer);
    SAFE_RELEASE(vertexBuffer);
    SAFE_RELEASE(vertexShader);
    SAFE_RELEASE(pixelShader);
    SAFE_RELEASE(inputLayout);
}

