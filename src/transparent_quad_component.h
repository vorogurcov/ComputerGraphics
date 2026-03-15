#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

struct TransparentQuadVertex {
    float Pos[3];
    float Color[4];
};

struct TransparentQuadComponent {
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11RasterizerState* rasterizerState = nullptr;
    ID3D11Buffer* mBuffer = nullptr;
    ID3D11Buffer* vpBuffer = nullptr;

    void Init(ID3D11Device* device);
    void RenderWithModel(ID3D11DeviceContext* context, const DirectX::XMMATRIX& modelMatrix, float aspectRatio, float camPitch, float camYaw);
    void Cleanup();

private:
    void CompileAndCreateShaders(ID3D11Device* device);
};
