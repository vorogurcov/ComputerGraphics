#include "utils.h"
#include "CUBE_COMPONENT.H"
#include <assert.h>
#include "dds_loader.h"

#include <cstring>
#include <string>

const char* g_CubeVertexShaderSource = R"(
cbuffer ModelBuffer : register(b0) {
    float4x4 model;
};

cbuffer SceneBuffer : register(b1) {
    float4x4 vp;
};

struct VS_INPUT {
    float3 Pos : POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    float4 worldPos = mul(model, float4(input.Pos, 1.0f));
    output.Pos = mul(vp, worldPos);
    output.Color = input.Color;
    output.UV = input.UV;
    return output;
}
)";

const char* g_CubePixelShaderSource = R"(
Texture2D colorTexture : register(t0);
SamplerState colorSampler : register(s0);

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET {
    float4 tex = colorTexture.Sample(colorSampler, input.UV);
    return lerp(input.Color, tex * input.Color, 0.7f);
}
)";

void CubeComponent::CompileAndCreateShaders(ID3D11Device* device) {
    HRESULT hr;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    hr = D3DCompile(g_CubeVertexShaderSource, strlen(g_CubeVertexShaderSource), "VS", nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            SAFE_RELEASE(errorBlob);
        }
        assert(false && "Failed to compile vertex shader!");
    }
    SAFE_RELEASE(errorBlob);

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
    assert(SUCCEEDED(hr));

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT numElements = ARRAYSIZE(layout);

    hr = device->CreateInputLayout(layout, numElements, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
    assert(SUCCEEDED(hr));
    SAFE_RELEASE(vsBlob);

    hr = D3DCompile(g_CubePixelShaderSource, strlen(g_CubePixelShaderSource), "PS", nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            SAFE_RELEASE(errorBlob);
        }
        assert(false && "Failed to compile pixel shader!");
    }
    SAFE_RELEASE(errorBlob);

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
    assert(SUCCEEDED(hr));
    SAFE_RELEASE(psBlob);
}


void CubeComponent::Init(ID3D11Device* device) {
    HRESULT hr;

    CubeVertex vertices[] = {
        { {-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },
        { {-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
        { { 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },
        { { 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },
        { {-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f} },
        { { 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
        { {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },
        { { 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },
        { {-0.5f, -0.5f,  0.5f}, {0.8f, 0.2f, 0.2f, 1.0f}, {0.0f, 1.0f} },
        { {-0.5f,  0.5f,  0.5f}, {0.8f, 0.6f, 0.2f, 1.0f}, {0.0f, 0.0f} },
        { {-0.5f,  0.5f, -0.5f}, {0.4f, 0.8f, 0.2f, 1.0f}, {1.0f, 0.0f} },
        { {-0.5f, -0.5f, -0.5f}, {0.2f, 0.4f, 0.8f, 1.0f}, {1.0f, 1.0f} },
        { { 0.5f, -0.5f, -0.5f}, {0.9f, 0.3f, 0.3f, 1.0f}, {0.0f, 1.0f} },
        { { 0.5f,  0.5f, -0.5f}, {0.3f, 0.9f, 0.3f, 1.0f}, {0.0f, 0.0f} },
        { { 0.5f,  0.5f,  0.5f}, {0.3f, 0.3f, 0.9f, 1.0f}, {1.0f, 0.0f} },
        { { 0.5f, -0.5f,  0.5f}, {0.9f, 0.9f, 0.3f, 1.0f}, {1.0f, 1.0f} },
        { {-0.5f,  0.5f, -0.5f}, {0.2f, 0.7f, 1.0f, 1.0f}, {0.0f, 1.0f} },
        { {-0.5f,  0.5f,  0.5f}, {0.2f, 0.5f, 0.9f, 1.0f}, {0.0f, 0.0f} },
        { { 0.5f,  0.5f,  0.5f}, {0.4f, 0.8f, 0.9f, 1.0f}, {1.0f, 0.0f} },
        { { 0.5f,  0.5f, -0.5f}, {0.6f, 0.9f, 1.0f, 1.0f}, {1.0f, 1.0f} },
        { {-0.5f, -0.5f,  0.5f}, {0.7f, 0.2f, 0.5f, 1.0f}, {0.0f, 1.0f} },
        { {-0.5f, -0.5f, -0.5f}, {0.5f, 0.2f, 0.7f, 1.0f}, {0.0f, 0.0f} },
        { { 0.5f, -0.5f, -0.5f}, {0.7f, 0.5f, 0.2f, 1.0f}, {1.0f, 0.0f} },
        { { 0.5f, -0.5f,  0.5f}, {0.9f, 0.7f, 0.3f, 1.0f}, {1.0f, 1.0f} },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(vertices);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;
    hr = device->CreateBuffer(&bd, &initData, &vertexBuffer);
    assert(SUCCEEDED(hr));

    WORD indices[] = {
        0,1,2, 0,2,3,
        4,5,6, 4,7,5,
        8,9,10, 8,10,11,
        12,13,14, 12,14,15,
        16,17,18, 16,18,19,
        20,21,22, 20,22,23
    };

    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth = sizeof(indices);
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iinitData = {};
    iinitData.pSysMem = indices;
    hr = device->CreateBuffer(&ibd, &iinitData, &indexBuffer);
    assert(SUCCEEDED(hr));

    D3D11_BUFFER_DESC mbd = {};
    mbd.ByteWidth = sizeof(CubeModelBuffer);
    mbd.Usage = D3D11_USAGE_DEFAULT;
    mbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device->CreateBuffer(&mbd, nullptr, &mBuffer);
    assert(SUCCEEDED(hr));

    D3D11_BUFFER_DESC vpbd = {};
    vpbd.ByteWidth = sizeof(CubeSceneBuffer);
    vpbd.Usage = D3D11_USAGE_DYNAMIC;
    vpbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    vpbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device->CreateBuffer(&vpbd, nullptr, &vpBuffer);
    assert(SUCCEEDED(hr));

    CompileAndCreateShaders(device);

    {
        DDSLoadedImage img;
        std::string err;
        const wchar_t* candidates[] = {
            L"assets/cube.dds",             
        };

        bool ok = false;
        for (size_t i = 0; i < ARRAYSIZE(candidates); ++i) {
            if (LoadDDSFromFile(candidates[i], img, &err)) {
                ok = true;
                std::string msg = "Cube DDS loaded from: ";
                char buf[260];
                size_t converted = 0;
                wcstombs_s(&converted, buf, candidates[i], _TRUNCATE);
                msg += buf;
                msg += "\n";
                OutputDebugStringA(msg.c_str());
                break;
            }
        }

        if (!ok || img.isCubemap) {
            if (!ok) OutputDebugStringA(("Cube DDS load failed: " + err + "\n").c_str());
            if (ok && img.isCubemap) OutputDebugStringA("Cube DDS expected Texture2D, got cubemap.\n");

            const uint32_t white = 0xffffffffu;
            D3D11_TEXTURE2D_DESC td = {};
            td.Width = 1;
            td.Height = 1;
            td.MipLevels = 1;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA init = {};
            init.pSysMem = &white;
            init.SysMemPitch = 4;
            init.SysMemSlicePitch = 4;

            hr = device->CreateTexture2D(&td, &init, &colorTexture);
            assert(SUCCEEDED(hr));

            D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
            srvd.Format = td.Format;
            srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvd.Texture2D.MostDetailedMip = 0;
            srvd.Texture2D.MipLevels = 1;
            hr = device->CreateShaderResourceView(colorTexture, &srvd, &colorTextureSRV);
            assert(SUCCEEDED(hr));
        }
        else {
            D3D11_TEXTURE2D_DESC td = {};
            td.Width = img.width;
            td.Height = img.height;
            td.MipLevels = img.mipCount;
            td.ArraySize = 1;
            td.Format = img.format;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            std::vector<D3D11_SUBRESOURCE_DATA> init;
            init.resize(img.subresources.size());
            for (uint32_t mip = 0; mip < img.mipCount; ++mip) {
                const DDSLoadedImageSubresource& sr = img.subresources[mip];
                init[mip].pSysMem = img.data.data() + sr.dataOffset;
                init[mip].SysMemPitch = sr.rowPitch;
                init[mip].SysMemSlicePitch = sr.slicePitch;
            }

            hr = device->CreateTexture2D(&td, init.data(), &colorTexture);
            assert(SUCCEEDED(hr));

            D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
            srvd.Format = td.Format;
            srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvd.Texture2D.MostDetailedMip = 0;
            srvd.Texture2D.MipLevels = td.MipLevels;
            hr = device->CreateShaderResourceView(colorTexture, &srvd, &colorTextureSRV);
            assert(SUCCEEDED(hr));
        }

        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_ANISOTROPIC;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.MaxAnisotropy = 16;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MinLOD = 0.0f;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&sd, &colorSampler);
        assert(SUCCEEDED(hr));
    }
}

void CubeComponent::Render(ID3D11DeviceContext* context, float time, float aspectRatio, float camPitch, float camYaw) {
    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetInputLayout(inputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);

    CubeModelBuffer mb;
    mb.m = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(1.0f, 1.0f, 0.0f, 0.0f), time);
    context->UpdateSubresource(mBuffer, 0, nullptr, &mb, 0, 0);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (SUCCEEDED(context->Map(vpBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
        CubeSceneBuffer* sb = (CubeSceneBuffer*)mappedResource.pData;

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

        context->Unmap(vpBuffer, 0);
    }

    ID3D11Buffer* cbs[] = { mBuffer, vpBuffer };
    context->VSSetConstantBuffers(0, 2, cbs);

    context->PSSetShaderResources(0, 1, &colorTextureSRV);
    context->PSSetSamplers(0, 1, &colorSampler);

    context->DrawIndexed(36, 0, 0);
}

void CubeComponent::RenderWithModel(ID3D11DeviceContext* context, const DirectX::XMMATRIX& modelMatrix, float aspectRatio, float camPitch, float camYaw) {
    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetInputLayout(inputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);

    CubeModelBuffer mb;
    mb.m = modelMatrix;
    context->UpdateSubresource(mBuffer, 0, nullptr, &mb, 0, 0);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (SUCCEEDED(context->Map(vpBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
        CubeSceneBuffer* sb = (CubeSceneBuffer*)mappedResource.pData;

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

        context->Unmap(vpBuffer, 0);
    }

    ID3D11Buffer* cbs[] = { mBuffer, vpBuffer };
    context->VSSetConstantBuffers(0, 2, cbs);

    context->PSSetShaderResources(0, 1, &colorTextureSRV);
    context->PSSetSamplers(0, 1, &colorSampler);

    context->DrawIndexed(36, 0, 0);
}

void CubeComponent::Cleanup() {
    SAFE_RELEASE(colorSampler);
    SAFE_RELEASE(colorTextureSRV);
    SAFE_RELEASE(colorTexture);

    SAFE_RELEASE(mBuffer);
    SAFE_RELEASE(vpBuffer);
    SAFE_RELEASE(indexBuffer);
    SAFE_RELEASE(vertexBuffer);
    SAFE_RELEASE(vertexShader);
    SAFE_RELEASE(pixelShader);
    SAFE_RELEASE(inputLayout);
}