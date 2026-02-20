#include "utils.h"
#include "CUBE_COMPONENT.H"
#include <assert.h>

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
};

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    float4 worldPos = mul(model, float4(input.Pos, 1.0f));
    output.Pos = mul(vp, worldPos);
    output.Color = input.Color;
    return output;
}
)";

const char* g_CubePixelShaderSource = R"(
struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

float4 main(PS_INPUT input) : SV_TARGET {
    return input.Color;
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
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
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
        { {-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.0f} },
        { {-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.0f} },
        { { 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.0f} },
        { { 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f, 1.0f} },
        { {-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f, 1.0f} },
        { {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 1.0f, 1.0f} },
        { { 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f, 1.0f} },
        { { 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f, 1.0f} }
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
        DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PI / 3.0f, aspectRatio, 0.1f, 100.0f);

        sb->vp = DirectX::XMMatrixMultiply(v, p);

        context->Unmap(vpBuffer, 0);
    }

    ID3D11Buffer* cbs[] = { mBuffer, vpBuffer };
    context->VSSetConstantBuffers(0, 2, cbs);

    context->DrawIndexed(36, 0, 0);
}

void CubeComponent::Cleanup() {
    SAFE_RELEASE(mBuffer);
    SAFE_RELEASE(vpBuffer);
    SAFE_RELEASE(indexBuffer);
    SAFE_RELEASE(vertexBuffer);
    SAFE_RELEASE(vertexShader);
    SAFE_RELEASE(pixelShader);
    SAFE_RELEASE(inputLayout);
}