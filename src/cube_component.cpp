#include "utils.h"
#include "CUBE_COMPONENT.H"
#include "dds_loader.h"

#include <assert.h>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>

namespace {

bool ReadTextFile(const char* path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) return false;
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size <= 0) return false;
    in.seekg(0, std::ios::beg);
    out.resize((size_t)size);
    in.read(&out[0], size);
    return in.good();
}

std::string DirectoryOf(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

class ShaderFileInclude final : public ID3DInclude {
public:
    explicit ShaderFileInclude(const std::string& baseDir) : baseDirectory(baseDir) {}

    STDMETHOD(Open)(D3D_INCLUDE_TYPE, LPCSTR pFileName, LPCVOID, LPCVOID* ppData, UINT* pBytes) override {
        if (!pFileName || !ppData || !pBytes) return E_INVALIDARG;
        std::string fullPath = baseDirectory + "/" + pFileName;
        std::string content;
        if (!ReadTextFile(fullPath.c_str(), content)) {
            return E_FAIL;
        }
        char* owned = new char[content.size()];
        memcpy(owned, content.data(), content.size());
        *ppData = owned;
        *pBytes = (UINT)content.size();
        return S_OK;
    }

    STDMETHOD(Close)(LPCVOID pData) override {
        delete[] (const char*)pData;
        return S_OK;
    }

private:
    std::string baseDirectory;
};

bool CompileShaderFromFile(
    const char* path,
    const char* entryPoint,
    const char* target,
    const D3D_SHADER_MACRO* macros,
    ID3DInclude* includeHandler,
    ID3DBlob** outBlob) {
    std::string source;
    if (!ReadTextFile(path, source)) {
        std::string msg = "Failed to read shader file: ";
        msg += path;
        msg += "\n";
        OutputDebugStringA(msg.c_str());
        return false;
    }

    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3DCompile(
        source.data(),
        source.size(),
        path,
        macros,
        includeHandler,
        entryPoint,
        target,
        0,
        0,
        outBlob,
        &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            SAFE_RELEASE(errorBlob);
        }
        return false;
    }
    SAFE_RELEASE(errorBlob);
    return true;
}

const char* SelectExistingPath(const char* const* candidates, size_t count) {
    std::string probe;
    for (size_t i = 0; i < count; ++i) {
        if (ReadTextFile(candidates[i], probe)) return candidates[i];
    }
    return nullptr;
}

void CreateFallbackTexture(
    ID3D11Device* device,
    uint32_t rgba,
    ID3D11Texture2D** outTexture,
    ID3D11ShaderResourceView** outSRV) {
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
    init.pSysMem = &rgba;
    init.SysMemPitch = 4;
    init.SysMemSlicePitch = 4;

    HRESULT hr = device->CreateTexture2D(&td, &init, outTexture);
    assert(SUCCEEDED(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MostDetailedMip = 0;
    srvd.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(*outTexture, &srvd, outSRV);
    assert(SUCCEEDED(hr));
}

bool LoadTexture2DFromDDS(
    ID3D11Device* device,
    const wchar_t* const* candidates,
    size_t candidateCount,
    ID3D11Texture2D** outTexture,
    ID3D11ShaderResourceView** outSRV,
    const char* textureName) {
    DDSLoadedImage img;
    std::string err;
    bool ok = false;
    for (size_t i = 0; i < candidateCount; ++i) {
        if (LoadDDSFromFile(candidates[i], img, &err)) {
            ok = true;
            std::string msg = textureName;
            msg += " DDS loaded.\n";
            OutputDebugStringA(msg.c_str());
            break;
        }
    }

    if (!ok || img.isCubemap) {
        if (!ok) {
            std::string msg = textureName;
            msg += " DDS load failed: ";
            msg += err;
            msg += "\n";
            OutputDebugStringA(msg.c_str());
        }
        if (ok && img.isCubemap) {
            std::string msg = textureName;
            msg += " DDS expected Texture2D, got cubemap.\n";
            OutputDebugStringA(msg.c_str());
        }
        return false;
    }

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

    HRESULT hr = device->CreateTexture2D(&td, init.data(), outTexture);
    assert(SUCCEEDED(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MostDetailedMip = 0;
    srvd.Texture2D.MipLevels = td.MipLevels;
    hr = device->CreateShaderResourceView(*outTexture, &srvd, outSRV);
    assert(SUCCEEDED(hr));
    return true;
}

} // namespace

void CubeComponent::CompileAndCreateShaders(ID3D11Device* device) {
    HRESULT hr;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;

    const char* const vsCandidates[] = {
        "src/shaders/cube_lit_vs.hlsl",
        "shaders/cube_lit_vs.hlsl"
    };
    const char* const psCandidates[] = {
        "src/shaders/cube_lit_ps.hlsl",
        "shaders/cube_lit_ps.hlsl"
    };
    const char* vsPath = SelectExistingPath(vsCandidates, ARRAYSIZE(vsCandidates));
    const char* psPath = SelectExistingPath(psCandidates, ARRAYSIZE(psCandidates));
    assert(vsPath && psPath);

    std::string includeDir = DirectoryOf(vsPath ? vsPath : ".");
    ShaderFileInclude includeHandler(includeDir);

    if (!CompileShaderFromFile(vsPath, "main", "vs_4_0", nullptr, &includeHandler, &vsBlob)) {
        assert(false && "Failed to compile cube vertex shader!");
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
    assert(SUCCEEDED(hr));

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = device->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
    assert(SUCCEEDED(hr));
    SAFE_RELEASE(vsBlob);

    const D3D_SHADER_MACRO psWithNormalMacros[] = {
        { "USE_NORMAL_MAP", "1" },
        { nullptr, nullptr }
    };
    if (!CompileShaderFromFile(psPath, "main", "ps_4_0", psWithNormalMacros, &includeHandler, &psBlob)) {
        assert(false && "Failed to compile cube pixel shader (normal map)!");
    }
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShaderWithNormalMap);
    assert(SUCCEEDED(hr));
    SAFE_RELEASE(psBlob);

    const D3D_SHADER_MACRO psNoNormalMacros[] = {
        { "USE_NORMAL_MAP", "0" },
        { nullptr, nullptr }
    };
    if (!CompileShaderFromFile(psPath, "main", "ps_4_0", psNoNormalMacros, &includeHandler, &psBlob)) {
        assert(false && "Failed to compile cube pixel shader (no normal map)!");
    }
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShaderNoNormalMap);
    assert(SUCCEEDED(hr));
    SAFE_RELEASE(psBlob);

    pixelShader = pixelShaderWithNormalMap;
}

void CubeComponent::Init(ID3D11Device* device) {
    HRESULT hr;

    const CubeVertex vertices[] = {
        // -Z
        { {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
        // +Z
        { {-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        // -X
        { {-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f} },
        { {-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} },
        { {-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f} },
        { {-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} },
        // +X
        { { 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },
        { { 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
        { { 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
        { { 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },
        // +Y
        { {-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
        // -Y
        { {-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(vertices);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;
    hr = device->CreateBuffer(&bd, &initData, &vertexBuffer);
    assert(SUCCEEDED(hr));

    const WORD indices[] = {
        0,1,2, 0,2,3,
        4,5,6, 4,6,7,
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

    const wchar_t* colorCandidates[] = {
        L"assets/cube.dds",
        L"src/assets/cube.dds",
    };
    const bool colorLoaded = LoadTexture2DFromDDS(device, colorCandidates, ARRAYSIZE(colorCandidates), &colorTexture, &colorTextureSRV, "Cube albedo");
    if (!colorLoaded) {
        CreateFallbackTexture(device, 0xffffffffu, &colorTexture, &colorTextureSRV);
    }

    const wchar_t* normalCandidates[] = {
        L"assets/cube_normal.dds",
        L"assets/cube_n.dds",
        L"assets/normal.dds",
        L"src/assets/cube_normal.dds",
    };
    hasNormalTextureFromFile = LoadTexture2DFromDDS(device, normalCandidates, ARRAYSIZE(normalCandidates), &normalTexture, &normalTextureSRV, "Cube normal");
    if (!hasNormalTextureFromFile) {
        // Flat tangent-space normal: (0.5, 0.5, 1.0).
        CreateFallbackTexture(device, 0xffff8080u, &normalTexture, &normalTextureSRV);
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

    frameLightingParams.lightCount = 1;
    frameLightingParams.lights[0].position = DirectX::XMFLOAT3(0.7f, 0.8f, -0.5f);
    frameLightingParams.lights[0].color = DirectX::XMFLOAT3(1.0f, 0.95f, 0.9f);
}

void CubeComponent::SetLightingParams(const CubeFrameLightingParams& params) {
    frameLightingParams = params;
    if (frameLightingParams.lightCount > MAX_POINT_LIGHTS) {
        frameLightingParams.lightCount = MAX_POINT_LIGHTS;
    }
}

void CubeComponent::Render(ID3D11DeviceContext* context, float time, float aspectRatio, float camPitch, float camYaw) {
    RenderWithModel(
        context,
        DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(1.0f, 1.0f, 0.0f, 0.0f), time),
        aspectRatio,
        camPitch,
        camYaw);
}

void CubeComponent::RenderWithModel(ID3D11DeviceContext* context, const DirectX::XMMATRIX& modelMatrix, float aspectRatio, float camPitch, float camYaw) {
    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetInputLayout(inputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(vertexShader, nullptr, 0);
    const bool useNormalMap = frameLightingParams.enableNormalMapping && (normalTextureSRV != nullptr);
    context->PSSetShader(useNormalMap ? pixelShaderWithNormalMap : pixelShaderNoNormalMap, nullptr, 0);

    CubeModelBuffer mb = {};
    mb.m = modelMatrix;
    mb.normalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, modelMatrix));
    context->UpdateSubresource(mBuffer, 0, nullptr, &mb, 0, 0);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (SUCCEEDED(context->Map(vpBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
        CubeSceneBuffer* sb = (CubeSceneBuffer*)mappedResource.pData;

        const DirectX::XMVECTOR cameraPos = DirectX::XMVectorSet(
            frameLightingParams.cameraPos.x,
            frameLightingParams.cameraPos.y,
            frameLightingParams.cameraPos.z,
            1.0f);
        const DirectX::XMMATRIX cameraRot = DirectX::XMMatrixRotationRollPitchYaw(camPitch, camYaw, 0.0f);
        const DirectX::XMVECTOR forward = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), cameraRot);
        const DirectX::XMVECTOR up = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), cameraRot);
        const DirectX::XMVECTOR focus = DirectX::XMVectorAdd(cameraPos, forward);

        const DirectX::XMMATRIX v = DirectX::XMMatrixLookAtLH(cameraPos, focus, up);
        const float nearZ = 0.1f;
        const float farZ = 100.0f;
        const DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PI / 3.0f, aspectRatio, farZ, nearZ);

        sb->vp = DirectX::XMMatrixMultiply(v, p);
        sb->cameraPos = DirectX::XMFLOAT4(frameLightingParams.cameraPos.x, frameLightingParams.cameraPos.y, frameLightingParams.cameraPos.z, 1.0f);
        sb->ambientColor = DirectX::XMFLOAT4(frameLightingParams.ambientColor.x, frameLightingParams.ambientColor.y, frameLightingParams.ambientColor.z, 1.0f);
        sb->lightCount = frameLightingParams.lightCount > MAX_POINT_LIGHTS ? MAX_POINT_LIGHTS : frameLightingParams.lightCount;
        for (UINT i = 0; i < MAX_POINT_LIGHTS; ++i) {
            sb->lights[i] = frameLightingParams.lights[i];
        }

        context->Unmap(vpBuffer, 0);
    }

    ID3D11Buffer* cbs[] = { mBuffer, vpBuffer };
    context->VSSetConstantBuffers(0, 2, cbs);
    context->PSSetConstantBuffers(0, 2, cbs);

    ID3D11ShaderResourceView* srvs[] = { colorTextureSRV, normalTextureSRV };
    context->PSSetShaderResources(0, 2, srvs);
    context->PSSetSamplers(0, 1, &colorSampler);

    context->DrawIndexed(36, 0, 0);
}

void CubeComponent::Cleanup() {
    SAFE_RELEASE(colorSampler);
    SAFE_RELEASE(normalTextureSRV);
    SAFE_RELEASE(normalTexture);
    SAFE_RELEASE(colorTextureSRV);
    SAFE_RELEASE(colorTexture);

    SAFE_RELEASE(mBuffer);
    SAFE_RELEASE(vpBuffer);
    SAFE_RELEASE(indexBuffer);
    SAFE_RELEASE(vertexBuffer);
    SAFE_RELEASE(vertexShader);
    SAFE_RELEASE(pixelShaderWithNormalMap);
    SAFE_RELEASE(pixelShaderNoNormalMap);
    pixelShader = nullptr;
    SAFE_RELEASE(inputLayout);
}