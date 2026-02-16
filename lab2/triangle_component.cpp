#include "utils.h"
#include "TRIANGLE_COMPONENT.H"
#include <assert.h>

const char* g_VertexShaderSource = R"(
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
    output.Pos = float4(input.Pos, 1.0f); // Позиция уже в NDC
    output.Color = input.Color;
    return output;
}
)";

const char* g_PixelShaderSource = R"(
struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

float4 main(PS_INPUT input) : SV_TARGET {
    return input.Color;
}
)";

void TriangleComponent::CompileAndCreateShaders(ID3D11Device* device) {
    HRESULT hr;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    hr = D3DCompile(g_VertexShaderSource, strlen(g_VertexShaderSource), "VS", nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
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

    hr = D3DCompile(g_PixelShaderSource, strlen(g_PixelShaderSource), "PS", nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
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


void TriangleComponent::Init(ID3D11Device* device) {
    HRESULT hr;

    Vertex vertices[] = {
        { {-0.5f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} },
        { { 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f} },
        { { 0.0f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f} }
    };

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(vertices);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    hr = device->CreateBuffer(&bd, &initData, &vertexBuffer);
    assert(SUCCEEDED(hr));

    CompileAndCreateShaders(device);
}

void TriangleComponent::Render(ID3D11DeviceContext* context) {
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

    context->IASetInputLayout(inputLayout);

    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->Draw(3, 0);
}

void TriangleComponent::Cleanup() {
    SAFE_RELEASE(vertexBuffer);
    SAFE_RELEASE(vertexShader);
    SAFE_RELEASE(pixelShader);
    SAFE_RELEASE(inputLayout);
}