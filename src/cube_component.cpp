#include "utils.h"
#include "CUBE_COMPONENT.H"
#include "dds_loader.h"

#include <assert.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
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

void CreateFallbackTextureArray2(
    ID3D11Device* device,
    uint32_t rgba0,
    uint32_t rgba1,
    ID3D11Texture2D** outTexture,
    ID3D11ShaderResourceView** outSRV) {
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 1;
    td.Height = 1;
    td.MipLevels = 1;
    td.ArraySize = 2;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init[2] = {};
    init[0].pSysMem = &rgba0;
    init[0].SysMemPitch = 4;
    init[0].SysMemSlicePitch = 4;
    init[1].pSysMem = &rgba1;
    init[1].SysMemPitch = 4;
    init[1].SysMemSlicePitch = 4;

    HRESULT hr = device->CreateTexture2D(&td, init, outTexture);
    assert(SUCCEEDED(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvd.Texture2DArray.MostDetailedMip = 0;
    srvd.Texture2DArray.MipLevels = td.MipLevels;
    srvd.Texture2DArray.FirstArraySlice = 0;
    srvd.Texture2DArray.ArraySize = td.ArraySize;
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

bool LoadDDSImageFromCandidates(
    const std::vector<std::wstring>& candidates,
    DDSLoadedImage& outImage,
    const char* textureName) {
    std::string err;
    for (const std::wstring& candidate : candidates) {
        if (LoadDDSFromFile(candidate.c_str(), outImage, &err)) {
            std::string msg = textureName;
            msg += " DDS loaded.\n";
            OutputDebugStringA(msg.c_str());
            return true;
        }
    }
    std::string msg = textureName;
    msg += " DDS load failed: ";
    msg += err;
    msg += "\n";
    OutputDebugStringA(msg.c_str());
    return false;
}

bool CreateTextureArray2FromDDS(
    ID3D11Device* device,
    const DDSLoadedImage& first,
    const DDSLoadedImage& second,
    ID3D11Texture2D** outTexture,
    ID3D11ShaderResourceView** outSRV) {
    if (first.isCubemap || second.isCubemap) return false;
    if (first.arraySize != 1 || second.arraySize != 1) return false;
    if (first.width != second.width || first.height != second.height) return false;
    if (first.format != second.format || first.mipCount != second.mipCount) return false;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = first.width;
    td.Height = first.height;
    td.MipLevels = first.mipCount;
    td.ArraySize = 2;
    td.Format = first.format;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    std::vector<D3D11_SUBRESOURCE_DATA> init;
    init.resize((size_t)td.ArraySize * td.MipLevels);
    for (uint32_t mip = 0; mip < td.MipLevels; ++mip) {
        const DDSLoadedImageSubresource& sr0 = first.subresources[mip];
        init[mip].pSysMem = first.data.data() + sr0.dataOffset;
        init[mip].SysMemPitch = sr0.rowPitch;
        init[mip].SysMemSlicePitch = sr0.slicePitch;

        const DDSLoadedImageSubresource& sr1 = second.subresources[mip];
        const uint32_t dstIndex = td.MipLevels + mip;
        init[dstIndex].pSysMem = second.data.data() + sr1.dataOffset;
        init[dstIndex].SysMemPitch = sr1.rowPitch;
        init[dstIndex].SysMemSlicePitch = sr1.slicePitch;
    }

    HRESULT hr = device->CreateTexture2D(&td, init.data(), outTexture);
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvd.Texture2DArray.MostDetailedMip = 0;
    srvd.Texture2DArray.MipLevels = td.MipLevels;
    srvd.Texture2DArray.FirstArraySlice = 0;
    srvd.Texture2DArray.ArraySize = td.ArraySize;
    hr = device->CreateShaderResourceView(*outTexture, &srvd, outSRV);
    if (FAILED(hr)) {
        SAFE_RELEASE(*outTexture);
        return false;
    }
    return true;
}

}

bool CubeComponent::CompileAndCreateShaders(ID3D11Device* device) {
    HRESULT hr;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* csBlob = nullptr;

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
    const std::vector<std::string> csCandidates = {
        JoinPathA(exeDir, "shaders\\cube_frustum_cull_cs.hlsl"),
        JoinPathA(exeDir, "src\\shaders\\cube_frustum_cull_cs.hlsl"),
        "shaders/cube_frustum_cull_cs.hlsl",
        "src/shaders/cube_frustum_cull_cs.hlsl"
    };
    const std::string vsPath = SelectExistingPath(vsCandidates);
    const std::string psPath = SelectExistingPath(psCandidates);
    const std::string csPath = SelectExistingPath(csCandidates);
    if (vsPath.empty() || psPath.empty() || csPath.empty()) {
        OutputDebugStringA("Cube shader files not found.\n");
        return false;
    }

    std::string includeDir = DirectoryOf(vsPath);
    ShaderFileInclude includeHandler(includeDir);

    if (!CompileShaderFromFile(vsPath.c_str(), "main", "vs_5_0", nullptr, &includeHandler, &vsBlob)) return false;

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

    if (!CompileShaderFromFile(psPath.c_str(), "main", "ps_5_0", nullptr, &includeHandler, &psBlob)) return false;
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
    SAFE_RELEASE(psBlob);
    if (FAILED(hr)) return false;

    if (!CompileShaderFromFile(csPath.c_str(), "main", "cs_5_0", nullptr, &includeHandler, &csBlob)) return false;
    hr = device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &cullComputeShader);
    SAFE_RELEASE(csBlob);
    if (FAILED(hr)) return false;

    return true;
}

void CubeComponent::InitPipelineStatsQueries(ID3D11Device* device) {
    D3D11_QUERY_DESC qd = {};
    qd.Query = D3D11_QUERY_PIPELINE_STATISTICS;
    qd.MiscFlags = 0;
    for (UINT i = 0; i < PIPELINE_QUERY_COUNT; ++i) {
        SAFE_RELEASE(pipelineStatsQueries[i]);
        HRESULT hr = device->CreateQuery(&qd, &pipelineStatsQueries[i]);
        assert(SUCCEEDED(hr));
    }
}

void CubeComponent::ReadPipelineStatsQueries(ID3D11DeviceContext* context) {
    while (nextReadFrame < curFrame) {
        const UINT queryIndex = (UINT)(nextReadFrame % PIPELINE_QUERY_COUNT);
        D3D11_QUERY_DATA_PIPELINE_STATISTICS stats = {};
        const HRESULT hr = context->GetData(
            pipelineStatsQueries[queryIndex],
            &stats,
            sizeof(stats),
            0);

        if (hr == S_FALSE) {
            break;
        }
        if (hr == S_OK) {
            gpuVisibleInstances = (UINT)(stats.IAPrimitives / CUBE_PRIMITIVES_PER_INSTANCE);
        }
        ++nextReadFrame;
    }
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

    D3D11_BUFFER_DESC gbd = {};
    gbd.ByteWidth = sizeof(CubeGeomBufferInst);
    gbd.Usage = D3D11_USAGE_DYNAMIC;
    gbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    gbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device->CreateBuffer(&gbd, nullptr, &geomInstBuffer);
    assert(SUCCEEDED(hr));

    D3D11_BUFFER_DESC vpbd = {};
    vpbd.ByteWidth = sizeof(CubeSceneBuffer);
    vpbd.Usage = D3D11_USAGE_DYNAMIC;
    vpbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    vpbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device->CreateBuffer(&vpbd, nullptr, &vpBuffer);
    assert(SUCCEEDED(hr));

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(CubeCullBuffer);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device->CreateBuffer(&cbd, nullptr, &cullBuffer);
    assert(SUCCEEDED(hr));

    if (!CompileAndCreateShaders(device)) {
        OutputDebugStringA("Cube init failed while compiling shaders.\n");
        isInitialized = false;
        return;
    }

    D3D11_BUFFER_DESC visibleIdsDesc = {};
    visibleIdsDesc.ByteWidth = sizeof(UINT) * MAX_CUBE_INSTANCES;
    visibleIdsDesc.Usage = D3D11_USAGE_DEFAULT;
    visibleIdsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    visibleIdsDesc.CPUAccessFlags = 0;
    visibleIdsDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    visibleIdsDesc.StructureByteStride = sizeof(UINT);
    hr = device->CreateBuffer(&visibleIdsDesc, nullptr, &visibleIdsBuffer);
    assert(SUCCEEDED(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC visibleIdsSRVDesc = {};
    visibleIdsSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    visibleIdsSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    visibleIdsSRVDesc.Buffer.FirstElement = 0;
    visibleIdsSRVDesc.Buffer.NumElements = MAX_CUBE_INSTANCES;
    hr = device->CreateShaderResourceView(visibleIdsBuffer, &visibleIdsSRVDesc, &visibleIdsSRV);
    assert(SUCCEEDED(hr));

    D3D11_UNORDERED_ACCESS_VIEW_DESC visibleIdsUAVDesc = {};
    visibleIdsUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    visibleIdsUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    visibleIdsUAVDesc.Buffer.FirstElement = 0;
    visibleIdsUAVDesc.Buffer.NumElements = MAX_CUBE_INSTANCES;
    hr = device->CreateUnorderedAccessView(visibleIdsBuffer, &visibleIdsUAVDesc, &visibleIdsUAV);
    assert(SUCCEEDED(hr));

    D3D11_BUFFER_DESC argsUavDesc = {};
    argsUavDesc.ByteWidth = sizeof(UINT) * 5;
    argsUavDesc.Usage = D3D11_USAGE_DEFAULT;
    argsUavDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    argsUavDesc.CPUAccessFlags = 0;
    argsUavDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    argsUavDesc.StructureByteStride = sizeof(UINT);
    hr = device->CreateBuffer(&argsUavDesc, nullptr, &indirectArgsUAVBuffer);
    assert(SUCCEEDED(hr));

    D3D11_UNORDERED_ACCESS_VIEW_DESC argsUAVDesc = {};
    argsUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    argsUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    argsUAVDesc.Buffer.FirstElement = 0;
    argsUAVDesc.Buffer.NumElements = 5;
    hr = device->CreateUnorderedAccessView(indirectArgsUAVBuffer, &argsUAVDesc, &indirectArgsUAV);
    assert(SUCCEEDED(hr));

    D3D11_BUFFER_DESC argsIndirectDesc = {};
    argsIndirectDesc.ByteWidth = sizeof(UINT) * 5;
    argsIndirectDesc.Usage = D3D11_USAGE_DEFAULT;
    argsIndirectDesc.BindFlags = 0;
    argsIndirectDesc.CPUAccessFlags = 0;
    argsIndirectDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    hr = device->CreateBuffer(&argsIndirectDesc, nullptr, &indirectArgsBuffer);
    assert(SUCCEEDED(hr));

    InitPipelineStatsQueries(device);

    const std::wstring exeDir = GetExecutableDirectoryW();
    const std::vector<std::wstring> colorCandidates0 = {
        JoinPathW(exeDir, L"assets\\cube.dds"),
        JoinPathW(exeDir, L"assets\\brick.dds"),
        JoinPathW(exeDir, L"src\\assets\\cube.dds"),
        L"assets\\cube.dds",
        L"assets\\brick.dds",
        L"src\\assets\\cube.dds",
    };
    const std::vector<std::wstring> colorCandidates1 = {
        JoinPathW(exeDir, L"assets\\cat.dds"),
        JoinPathW(exeDir, L"assets\\cube_alt.dds"),
        JoinPathW(exeDir, L"src\\assets\\cat.dds"),
        L"assets\\cat.dds",
        L"assets\\cube_alt.dds",
        L"src\\assets\\cat.dds",
    };
    DDSLoadedImage tex0 = {};
    DDSLoadedImage tex1 = {};
    const bool firstLoaded = LoadDDSImageFromCandidates(colorCandidates0, tex0, "Cube albedo[0]");
    const bool secondLoaded = LoadDDSImageFromCandidates(colorCandidates1, tex1, "Cube albedo[1]");
    const bool colorArrayLoaded = firstLoaded && secondLoaded &&
        CreateTextureArray2FromDDS(device, tex0, tex1, &colorTextureArray, &colorTextureArraySRV);
    if (!colorArrayLoaded) {
        CreateFallbackTextureArray2(device, 0xffa0a0a0u, 0xff4ca3ffu, &colorTextureArray, &colorTextureArraySRV);
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

namespace {

struct WorldAABB {
    DirectX::XMFLOAT3 minP;
    DirectX::XMFLOAT3 maxP;
};

WorldAABB ComputeCubeAabbWS(const DirectX::XMMATRIX& model) {
    using namespace DirectX;
    const XMVECTOR localCorners[8] = {
        XMVectorSet(-0.5f, -0.5f, -0.5f, 1.0f),
        XMVectorSet( 0.5f, -0.5f, -0.5f, 1.0f),
        XMVectorSet(-0.5f,  0.5f, -0.5f, 1.0f),
        XMVectorSet( 0.5f,  0.5f, -0.5f, 1.0f),
        XMVectorSet(-0.5f, -0.5f,  0.5f, 1.0f),
        XMVectorSet( 0.5f, -0.5f,  0.5f, 1.0f),
        XMVectorSet(-0.5f,  0.5f,  0.5f, 1.0f),
        XMVectorSet( 0.5f,  0.5f,  0.5f, 1.0f),
    };

    XMVECTOR minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
    XMVECTOR maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);
    for (int i = 0; i < 8; ++i) {
        const XMVECTOR p = XMVector3TransformCoord(localCorners[i], model);
        minV = XMVectorMin(minV, p);
        maxV = XMVectorMax(maxV, p);
    }

    WorldAABB box = {};
    XMStoreFloat3(&box.minP, minV);
    XMStoreFloat3(&box.maxP, maxV);
    return box;
}

void NormalizePlane(DirectX::XMFLOAT4& p) {
    const float len = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
    if (len > 1e-6f) {
        p.x /= len;
        p.y /= len;
        p.z /= len;
        p.w /= len;
    }
}

void ExtractFrustumPlanes(const DirectX::XMMATRIX& viewProj, DirectX::XMFLOAT4 outPlanes[6]) {
    using namespace DirectX;
    XMFLOAT4X4 m = {};
    XMStoreFloat4x4(&m, viewProj);

    outPlanes[0] = XMFLOAT4(m._14 + m._11, m._24 + m._21, m._34 + m._31, m._44 + m._41);
    outPlanes[1] = XMFLOAT4(m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41);
    outPlanes[2] = XMFLOAT4(m._14 + m._12, m._24 + m._22, m._34 + m._32, m._44 + m._42);
    outPlanes[3] = XMFLOAT4(m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42);
    outPlanes[4] = XMFLOAT4(m._13, m._23, m._33, m._43);
    outPlanes[5] = XMFLOAT4(m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43);

    for (int i = 0; i < 6; ++i) {
        NormalizePlane(outPlanes[i]);
    }
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
    CubeInstanceData single = {};
    single.model = modelMatrix;
    single.shininess = 32.0f;
    single.textureId = 0;
    single.useNormalMap = true;
    RenderInstanced(context, &single, 1, aspectRatio, camPitch, camYaw);
}

void CubeComponent::RenderInstanced(ID3D11DeviceContext* context, const CubeInstanceData* instances, UINT instanceCount, float aspectRatio, float camPitch, float camYaw) {
    if (!isInitialized || !vertexShader || !pixelShader || !cullComputeShader || !inputLayout || !instances || instanceCount == 0) return;
    const UINT totalCount = (std::min)(instanceCount, (UINT)MAX_CUBE_INSTANCES);

    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetInputLayout(inputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);

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
    const DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PI / 3.0f, aspectRatio, nearZ, farZ);
    const DirectX::XMMATRIX vp = DirectX::XMMatrixMultiply(v, p);

    CubeGeomBufferInst geomInst = {};
    CubeCullBuffer cullData = {};
    ExtractFrustumPlanes(vp, cullData.frustumPlanes);
    cullData.objectCount = totalCount;

    for (UINT i = 0; i < totalCount; ++i) {
        const WorldAABB aabb = ComputeCubeAabbWS(instances[i].model);
        cullData.bbMin[i] = DirectX::XMFLOAT4(aabb.minP.x, aabb.minP.y, aabb.minP.z, 0.0f);
        cullData.bbMax[i] = DirectX::XMFLOAT4(aabb.maxP.x, aabb.maxP.y, aabb.maxP.z, 0.0f);

        CubeGeomBuffer& geom = geomInst.geomBuffer[i];
        geom.m = instances[i].model;
        geom.normalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, instances[i].model));
        const bool useNormalMap = instances[i].useNormalMap && frameLightingParams.enableNormalMapping && hasNormalTextureFromFile;
        const float textureId = (float)((instances[i].textureId > 1u) ? 1u : instances[i].textureId);
        geom.shineSpeedTexIdNM = DirectX::XMFLOAT4((std::max)(instances[i].shininess, 1.0f), 0.0f, textureId, useNormalMap ? 1.0f : 0.0f);
    }

    lastVisibleInstanceCount = 0;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(context->Map(geomInstBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, &geomInst, sizeof(geomInst));
        context->Unmap(geomInstBuffer, 0);
    }
    if (SUCCEEDED(context->Map(cullBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, &cullData, sizeof(cullData));
        context->Unmap(cullBuffer, 0);
    }
    if (SUCCEEDED(context->Map(vpBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        CubeSceneBuffer* sb = (CubeSceneBuffer*)mapped.pData;
        sb->vp = vp;
        sb->cameraPos = DirectX::XMFLOAT4(frameLightingParams.cameraPos.x, frameLightingParams.cameraPos.y, frameLightingParams.cameraPos.z, 1.0f);
        sb->ambientColor = DirectX::XMFLOAT4(frameLightingParams.ambientColor.x, frameLightingParams.ambientColor.y, frameLightingParams.ambientColor.z, 1.0f);
        sb->lightCount = frameLightingParams.lightCount > MAX_POINT_LIGHTS ? MAX_POINT_LIGHTS : frameLightingParams.lightCount;
        for (UINT i = 0; i < MAX_POINT_LIGHTS; ++i) {
            sb->lights[i] = frameLightingParams.lights[i];
        }
        context->Unmap(vpBuffer, 0);
    }

    ID3D11Buffer* drawArgsCb = nullptr;
    ID3D11ShaderResourceView* nullSrv = nullptr;
    context->VSSetShaderResources(2, 1, &nullSrv);
    context->PSSetShaderResources(2, 1, &nullSrv);

    const UINT drawArgs[5] = { 36u, 0u, 0u, 0u, 0u };
    context->UpdateSubresource(indirectArgsUAVBuffer, 0, nullptr, drawArgs, 0, 0);

    ID3D11Buffer* cullCb[] = { cullBuffer };
    context->CSSetConstantBuffers(0, 1, cullCb);
    ID3D11UnorderedAccessView* cullUavs[] = { indirectArgsUAV, visibleIdsUAV };
    UINT initialCounts[] = { 0, 0 };
    context->CSSetUnorderedAccessViews(0, 2, cullUavs, initialCounts);
    context->CSSetShader(cullComputeShader, nullptr, 0);
    context->Dispatch((totalCount + 63u) / 64u, 1, 1);
    context->CSSetShader(nullptr, nullptr, 0);

    ID3D11UnorderedAccessView* nullUAVs[2] = { nullptr, nullptr };
    context->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
    context->CSSetConstantBuffers(0, 1, &drawArgsCb);

    context->CopyResource(indirectArgsBuffer, indirectArgsUAVBuffer);

    ID3D11Buffer* cbs[] = { geomInstBuffer, vpBuffer };
    context->VSSetConstantBuffers(0, 2, cbs);
    context->PSSetConstantBuffers(0, 2, cbs);

    ID3D11ShaderResourceView* srvs[] = { colorTextureArraySRV, normalTextureSRV, visibleIdsSRV };
    context->VSSetShaderResources(2, 1, &visibleIdsSRV);
    context->PSSetShaderResources(0, 3, srvs);
    context->PSSetSamplers(0, 1, &colorSampler);

    const UINT queryIndex = (UINT)(curFrame % PIPELINE_QUERY_COUNT);
    ID3D11Query* pipelineQuery = pipelineStatsQueries[queryIndex];
    context->Begin(pipelineQuery);
    context->DrawIndexedInstancedIndirect(indirectArgsBuffer, 0);
    context->End(pipelineQuery);

    ++curFrame;
    ReadPipelineStatsQueries(context);
    lastVisibleInstanceCount = gpuVisibleInstances;

    context->VSSetShaderResources(2, 1, &nullSrv);
    context->PSSetShaderResources(2, 1, &nullSrv);
}

void CubeComponent::Cleanup() {
    isInitialized = false;
    SAFE_RELEASE(colorSampler);
    SAFE_RELEASE(normalTextureSRV);
    SAFE_RELEASE(normalTexture);
    SAFE_RELEASE(colorTextureArraySRV);
    SAFE_RELEASE(colorTextureArray);

    for (UINT i = 0; i < PIPELINE_QUERY_COUNT; ++i) {
        SAFE_RELEASE(pipelineStatsQueries[i]);
    }
    SAFE_RELEASE(indirectArgsBuffer);
    SAFE_RELEASE(indirectArgsUAV);
    SAFE_RELEASE(indirectArgsUAVBuffer);
    SAFE_RELEASE(visibleIdsUAV);
    SAFE_RELEASE(visibleIdsSRV);
    SAFE_RELEASE(visibleIdsBuffer);
    SAFE_RELEASE(cullBuffer);
    SAFE_RELEASE(geomInstBuffer);
    SAFE_RELEASE(vpBuffer);
    SAFE_RELEASE(indexBuffer);
    SAFE_RELEASE(vertexBuffer);
    SAFE_RELEASE(cullComputeShader);
    SAFE_RELEASE(vertexShader);
    SAFE_RELEASE(pixelShader);
    SAFE_RELEASE(inputLayout);
}