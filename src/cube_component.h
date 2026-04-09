#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <cstdint>

#pragma comment(lib, "d3dcompiler.lib") 

static const UINT MAX_POINT_LIGHTS = 8;
static const UINT MAX_CUBE_INSTANCES = 100;

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

struct CubeGeomBuffer {
    DirectX::XMMATRIX m;
    DirectX::XMMATRIX normalMatrix;
    DirectX::XMFLOAT4 shineSpeedTexIdNM;
};

struct CubeGeomBufferInst {
    CubeGeomBuffer geomBuffer[MAX_CUBE_INSTANCES];
};

struct CubeCullBuffer {
    DirectX::XMFLOAT4 frustumPlanes[6];
    UINT objectCount = 0;
    float _pad0[3] = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 bbMin[MAX_CUBE_INSTANCES];
    DirectX::XMFLOAT4 bbMax[MAX_CUBE_INSTANCES];
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

struct CubeInstanceData {
    DirectX::XMMATRIX model = DirectX::XMMatrixIdentity();
    float shininess = 32.0f;
    uint32_t textureId = 0;
    bool useNormalMap = true;
};

struct CubeComponent {
    static const UINT PIPELINE_QUERY_COUNT = 10;
    static const UINT CUBE_PRIMITIVES_PER_INSTANCE = 12;

    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11ComputeShader* cullComputeShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;

    ID3D11Buffer* geomInstBuffer = nullptr;
    ID3D11Buffer* cullBuffer = nullptr;
    ID3D11Buffer* vpBuffer = nullptr;
    ID3D11Buffer* visibleIdsBuffer = nullptr;
    ID3D11ShaderResourceView* visibleIdsSRV = nullptr;
    ID3D11UnorderedAccessView* visibleIdsUAV = nullptr;
    ID3D11Buffer* indirectArgsUAVBuffer = nullptr;
    ID3D11UnorderedAccessView* indirectArgsUAV = nullptr;
    ID3D11Buffer* indirectArgsBuffer = nullptr;

    ID3D11Texture2D* colorTextureArray = nullptr;
    ID3D11ShaderResourceView* colorTextureArraySRV = nullptr;
    ID3D11Texture2D* normalTexture = nullptr;
    ID3D11ShaderResourceView* normalTextureSRV = nullptr;
    ID3D11SamplerState* colorSampler = nullptr;
    ID3D11Query* pipelineStatsQueries[PIPELINE_QUERY_COUNT] = {};

    void Init(ID3D11Device* device);
    void SetLightingParams(const CubeFrameLightingParams& params);
    void Render(ID3D11DeviceContext* context, float time, float aspectRatio, float camPitch, float camYaw);
    void RenderWithModel(ID3D11DeviceContext* context, const DirectX::XMMATRIX& modelMatrix, float aspectRatio, float camPitch, float camYaw);
    void RenderInstanced(ID3D11DeviceContext* context, const CubeInstanceData* instances, UINT instanceCount, float aspectRatio, float camPitch, float camYaw);
    void Cleanup();

private:
    bool CompileAndCreateShaders(ID3D11Device* device);
    void InitPipelineStatsQueries(ID3D11Device* device);
    void ReadPipelineStatsQueries(ID3D11DeviceContext* context);
    bool isInitialized = false;
    bool hasNormalTextureFromFile = false;
    UINT lastVisibleInstanceCount = 0;
    UINT gpuVisibleInstances = 0;
    UINT64 curFrame = 0;
    UINT64 nextReadFrame = 0;
    CubeFrameLightingParams frameLightingParams = {};
};