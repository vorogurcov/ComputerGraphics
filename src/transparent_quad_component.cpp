#include "utils.h"
#include "transparent_quad_component.h"
#include <d3dcompiler.h>
#include <assert.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

struct TransparentModelBuffer { DirectX::XMMATRIX m; };
struct TransparentSceneBuffer { DirectX::XMMATRIX vp; };

const char* g_TransparentQuadVS = R"(
cbuffer ModelBuffer : register(b0) { float4x4 model; }
cbuffer SceneBuffer : register(b1) { float4x4 vp; }
struct VS_INPUT { float3 Pos : POSITION; float4 Color : COLOR; };
struct VS_OUTPUT { float4 Pos : SV_POSITION; float4 Color : COLOR; };
VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.Pos = mul(vp, mul(model, float4(input.Pos, 1.0f)));
    output.Color = input.Color;
    return output;
}
)";

const char* g_TransparentQuadPS = R"(
struct PS_INPUT { float4 Pos : SV_POSITION; float4 Color : COLOR; };
float4 main(PS_INPUT input) : SV_TARGET { return input.Color; }
)";

void TransparentQuadComponent::CompileAndCreateShaders(ID3D11Device* device) {
    ID3DBlob* vsBlob = nullptr, * psBlob = nullptr, * err = nullptr;
    HRESULT hr = D3DCompile(g_TransparentQuadVS, (SIZE_T)strlen(g_TransparentQuadVS), "VS", nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsBlob, &err);
    if (FAILED(hr)) { if (err) { OutputDebugStringA((char*)err->GetBufferPointer()); SAFE_RELEASE(err); } assert(false); }
    SAFE_RELEASE(err);
    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    device->CreateInputLayout(layout, (UINT)ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
    SAFE_RELEASE(vsBlob);

    hr = D3DCompile(g_TransparentQuadPS, (SIZE_T)strlen(g_TransparentQuadPS), "PS", nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, &err);
    if (FAILED(hr)) { if (err) { SAFE_RELEASE(err); } assert(false); }
    SAFE_RELEASE(err);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
    SAFE_RELEASE(psBlob);
}

void TransparentQuadComponent::Init(ID3D11Device* device) {
    TransparentQuadVertex verts[] = {
        { {-0.5f, -0.5f, 0.0f}, {0.15f, 0.85f, 0.95f, 0.45f} },
        { { 0.5f, -0.5f, 0.0f}, {0.15f, 0.85f, 0.95f, 0.45f} },
        { { 0.5f,  0.5f, 0.0f}, {0.95f, 0.35f, 0.80f, 0.45f} },
        { {-0.5f,  0.5f, 0.0f}, {0.95f, 0.35f, 0.80f, 0.45f} }
    };
    WORD indices[] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(verts);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = verts;
    device->CreateBuffer(&bd, &init, &vertexBuffer);

    bd.ByteWidth = sizeof(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    init.pSysMem = indices;
    device->CreateBuffer(&bd, &init, &indexBuffer);

    bd.ByteWidth = sizeof(TransparentModelBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.Usage = D3D11_USAGE_DEFAULT;
    device->CreateBuffer(&bd, nullptr, &mBuffer);

    bd.ByteWidth = sizeof(TransparentSceneBuffer);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&bd, nullptr, &vpBuffer);

    D3D11_RASTERIZER_DESC rsd = {};
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_NONE;
    rsd.DepthClipEnable = TRUE;
    device->CreateRasterizerState(&rsd, &rasterizerState);

    CompileAndCreateShaders(device);
}

void TransparentQuadComponent::RenderWithModel(ID3D11DeviceContext* context, const DirectX::XMMATRIX& modelMatrix, float aspectRatio, float camPitch, float camYaw) {
    UINT stride = sizeof(TransparentQuadVertex), offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetInputLayout(inputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);
    context->RSSetState(rasterizerState);

    TransparentModelBuffer mb;
    mb.m = modelMatrix;
    context->UpdateSubresource(mBuffer, 0, nullptr, &mb, 0, 0);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(context->Map(vpBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        TransparentSceneBuffer* sb = (TransparentSceneBuffer*)mapped.pData;
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
    context->DrawIndexed(6, 0, 0);
    context->RSSetState(nullptr);
}

void TransparentQuadComponent::Cleanup() {
    SAFE_RELEASE(rasterizerState);
    SAFE_RELEASE(mBuffer);
    SAFE_RELEASE(vpBuffer);
    SAFE_RELEASE(indexBuffer);
    SAFE_RELEASE(vertexBuffer);
    SAFE_RELEASE(vertexShader);
    SAFE_RELEASE(pixelShader);
    SAFE_RELEASE(inputLayout);
}
