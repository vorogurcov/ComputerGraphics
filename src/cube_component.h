#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#pragma comment(lib, "d3dcompiler.lib") 

struct CubeVertex {
    float Pos[3];
    float Color[4];
    float UV[2];
};

struct CubeModelBuffer {
    DirectX::XMMATRIX m;
};

struct CubeSceneBuffer {
    DirectX::XMMATRIX vp;
};

struct CubeComponent {
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;

    ID3D11Buffer* mBuffer = nullptr;
    ID3D11Buffer* vpBuffer = nullptr;

    ID3D11Texture2D* colorTexture = nullptr;
    ID3D11ShaderResourceView* colorTextureSRV = nullptr;
    ID3D11SamplerState* colorSampler = nullptr;

    void Init(ID3D11Device* device);
    void Render(ID3D11DeviceContext* context, float time, float aspectRatio, float camPitch, float camYaw);
    void Cleanup();

private:
    void CompileAndCreateShaders(ID3D11Device* device);
};