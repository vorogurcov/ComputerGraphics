#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>


#pragma comment(lib, "d3dcompiler.lib") 

struct Vertex {
    float Pos[3]; 
    float Color[4]; 
};

struct TriangleComponent {
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;

    void Init(ID3D11Device* device);
    void Render(ID3D11DeviceContext* context);
    void Cleanup();

private:
    void CompileAndCreateShaders(ID3D11Device* device);
};