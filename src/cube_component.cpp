#include "utils.h"
#include "CUBE_COMPONENT.H"
#include "dds_loader.h"

#include <assert.h>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <windows.h>

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

std::string GetExecutableDirectoryA() {
    char path[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return ".";
    return DirectoryOf(std::string(path, path + len));
}

std::wstring GetExecutableDirectoryW() {
    wchar_t path[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return L".";
    std::wstring s(path, path + len);
    const size_t slash = s.find_last_of(L"/\\");
    if (slash == std::wstring::npos) return L".";
    return s.substr(0, slash);
}

std::string JoinPathA(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    char back = a.back();
    if (back == '\\' || back == '/') return a + b;
    return a + "\\" + b;
}

std::wstring JoinPathW(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    wchar_t back = a.back();
    if (back == L'\\' || back == L'/') return a + b;
    return a + L"\\" + b;
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

std::string SelectExistingPath(const std::vector<std::string>& candidates) {
    std::string probe;
    for (const std::string& candidate : candidates) {
        if (ReadTextFile(candidate.c_str(), probe)) return candidate;
    }
    return std::string();
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
    const std::vector<std::wstring>& candidates,
    ID3D11Texture2D** outTexture,
    ID3D11ShaderResourceView** outSRV,
    const char* textureName) {
    DDSLoadedImage img;
    std::string err;
    bool ok = false;
    for (const std::wstring& candidate : candidates) {
        if (LoadDDSFromFile(candidate.c_str(), img, &err)) {
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

}

bool CubeComponent::CompileAndCreateShaders(ID3D11Device* device) {
    HRESULT hr;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;

    const std::string exeDir = GetExecutableDirectoryA();
    const std::vector<std::string> vsCandidates = {
        JoinPathA(exeDir, "shaders\\cube_lit_vs.hlsl"),
        JoinPathA(exeDir, "src\\shaders\\cube_lit_vs.hlsl"),
        "shaders/cube_lit_vs.hlsl",
        "src/shaders/cube_lit_vs.hlsl"
    };
    const std::vector<std::string> psCandidates = {
        JoinPathA(exeDir, "shaders\\cube_lit_ps.hlsl"),
        JoinPathA(exeDir, "src\\shaders\\cube_lit_ps.hlsl"),
        "shaders/cube_lit_ps.hlsl",
        "src/shaders/cube_lit_ps.hlsl"
    };
    const std::string vsPath = SelectExistingPath(vsCandidates);
    const std::string psPath = SelectExistingPath(psCandidates);
    if (vsPath.empty() || psPath.empty()) {
        OutputDebugStringA("Cube shader files not found.\n");
        return false;
    }

    std::string includeDir = DirectoryOf(vsPath);
    ShaderFileInclude includeHandler(includeDir);

    if (!CompileShaderFromFile(vsPath.c_str(), "main", "vs_4_0", nullptr, &includeHandler, &vsBlob)) return false;

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
    if (FAILED(hr)) { SAFE_RELEASE(vsBlob); return false; }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = device->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
    SAFE_RELEASE(vsBlob);
    if (FAILED(hr)) return false;

    const D3D_SHADER_MACRO psWithNormalMacros[] = {
        { "USE_NORMAL_MAP", "1" },
        { nullptr, nullptr }
    };
    if (!CompileShaderFromFile(psPath.c_str(), "main", "ps_4_0", psWithNormalMacros, &includeHandler, &psBlob)) return false;
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShaderWithNormalMap);
    SAFE_RELEASE(psBlob);
    if (FAILED(hr)) return false;

    const D3D_SHADER_MACRO psNoNormalMacros[] = {
        { "USE_NORMAL_MAP", "0" },
        { nullptr, nullptr }
    };
    if (!CompileShaderFromFile(psPath.c_str(), "main", "ps_4_0", psNoNormalMacros, &includeHandler, &psBlob)) return false;
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShaderNoNormalMap);
    SAFE_RELEASE(psBlob);
    if (FAILED(hr)) return false;

    pixelShader = pixelShaderWithNormalMap;
    return true;
}

void CubeComponent::Init(ID3D11Device* device) {
    HRESULT hr;

    const CubeVertex vertices[] = {
        { {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f} },
        { {-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} },
        { {-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f} },
        { {-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} },
        { { 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },
        { { 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
        { { 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
        { { 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },
        { {-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
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

    if (!CompileAndCreateShaders(device)) {
        OutputDebugStringA("Cube init failed while compiling shaders.\n");
        isInitialized = false;
        return;
    }

    const std::wstring exeDir = GetExecutableDirectoryW();
    const std::vector<std::wstring> colorCandidates = {
        JoinPathW(exeDir, L"assets\\cube.dds"),
        JoinPathW(exeDir, L"src\\assets\\cube.dds"),
        L"assets\\cube.dds",
        L"src\\assets\\cube.dds",
    };
    const bool colorLoaded = LoadTexture2DFromDDS(device, colorCandidates, &colorTexture, &colorTextureSRV, "Cube albedo");
    if (!colorLoaded) {
        CreateFallbackTexture(device, 0xffffffffu, &colorTexture, &colorTextureSRV);
    }

    const std::vector<std::wstring> normalCandidates = {
        JoinPathW(exeDir, L"assets\\cube_normal.dds"),
        JoinPathW(exeDir, L"assets\\cube_n.dds"),
        JoinPathW(exeDir, L"assets\\normal.dds"),
        JoinPathW(exeDir, L"src\\assets\\cube_normal.dds"),
        L"assets\\cube_normal.dds",
        L"assets\\cube_n.dds",
        L"assets\\normal.dds",
        L"src\\assets\\cube_normal.dds",
    };
    hasNormalTextureFromFile = LoadTexture2DFromDDS(device, normalCandidates, &normalTexture, &normalTextureSRV, "Cube normal");
    if (!hasNormalTextureFromFile) {
        CreateFallbackTexture(device, 0xffff8080u, &normalTexture, &normalTextureSRV);
    }
    OutputDebugStringA(hasNormalTextureFromFile ? "Normal map: loaded from file\n" : "Normal map: fallback\n");

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
    isInitialized = true;
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
    if (!isInitialized || !vertexShader || !inputLayout || !pixelShaderWithNormalMap || !pixelShaderNoNormalMap) return;

    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetInputLayout(inputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(vertexShader, nullptr, 0);
    const bool useNormalMap = frameLightingParams.enableNormalMapping && hasNormalTextureFromFile;
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
    isInitialized = false;
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