#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#pragma comment(lib, "d3dcompiler.lib")

struct SkyboxVertex {
    float Pos[3];
};

struct SkyboxSceneBuffer {
    DirectX::XMMATRIX vp;
    DirectX::XMFLOAT4 camPos;
};

struct SkyboxComponent {
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11Buffer* sceneBuffer = nullptr;

    ID3D11Texture2D* cubemapTexture = nullptr;
    ID3D11ShaderResourceView* cubemapSRV = nullptr;
    ID3D11SamplerState* sampler = nullptr;

    ID3D11DepthStencilState* depthState = nullptr;
    ID3D11RasterizerState* rasterState = nullptr;

    void Init(ID3D11Device* device);
    void Render(ID3D11DeviceContext* context, float aspectRatio, float camPitch, float camYaw);
    void Cleanup();

private:
    void CompileAndCreateShaders(ID3D11Device* device);
    void CreatePipelineStates(ID3D11Device* device);
    void LoadCubemapDDS(ID3D11Device* device);
};

