#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#pragma comment(lib, "d3dcompiler.lib") 

static const UINT MAX_POINT_LIGHTS = 8;

struct CubeVertex {
    float Pos[3];
    float Normal[3];
    float UV[2];
    float Tangent[3];
};

struct CubePointLight {
    DirectX::XMFLOAT3 position;
    float _pad0 = 0.0f;
    DirectX::XMFLOAT3 color;
    float _pad1 = 0.0f;
};

struct CubeModelBuffer {
    DirectX::XMMATRIX m;
    DirectX::XMMATRIX normalMatrix;
};

struct CubeSceneBuffer {
    DirectX::XMMATRIX vp;
    DirectX::XMFLOAT4 cameraPos;
    DirectX::XMFLOAT4 ambientColor;
    UINT lightCount = 0;
    float _pad0[3] = { 0.0f, 0.0f, 0.0f };
    CubePointLight lights[MAX_POINT_LIGHTS];
};

struct CubeFrameLightingParams {
    DirectX::XMFLOAT3 cameraPos = DirectX::XMFLOAT3(0.0f, 0.0f, -3.0f);
    DirectX::XMFLOAT3 ambientColor = DirectX::XMFLOAT3(0.1f, 0.1f, 0.1f);
    UINT lightCount = 0;
    CubePointLight lights[MAX_POINT_LIGHTS] = {};
    bool enableNormalMapping = true;
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
    ID3D11Texture2D* normalTexture = nullptr;
    ID3D11ShaderResourceView* normalTextureSRV = nullptr;
    ID3D11SamplerState* colorSampler = nullptr;

    void Init(ID3D11Device* device);
    void SetLightingParams(const CubeFrameLightingParams& params);
    void Render(ID3D11DeviceContext* context, float time, float aspectRatio, float camPitch, float camYaw);
    void RenderWithModel(ID3D11DeviceContext* context, const DirectX::XMMATRIX& modelMatrix, float aspectRatio, float camPitch, float camYaw);
    void Cleanup();

private:
    bool CompileAndCreateShaders(ID3D11Device* device);
    bool isInitialized = false;
    bool hasNormalTextureFromFile = false;
    CubeFrameLightingParams frameLightingParams = {};
    ID3D11PixelShader* pixelShaderWithNormalMap = nullptr;
    ID3D11PixelShader* pixelShaderNoNormalMap = nullptr;
};