#include "dds_loader.h"

#include <algorithm>
#include <limits>

#include <windows.h>

namespace {

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

    constexpr uint32_t MakeFourCC(char a, char b, char c, char d) {
        return (uint32_t)(uint8_t)a |
            ((uint32_t)(uint8_t)b << 8) |
            ((uint32_t)(uint8_t)c << 16) |
            ((uint32_t)(uint8_t)d << 24);
    }

    constexpr uint32_t DDS_MAGIC = MakeFourCC('D', 'D', 'S', ' ');

    constexpr uint32_t DDPF_FOURCC = 0x00000004u;
    constexpr uint32_t DDPF_RGB = 0x00000040u;
    constexpr uint32_t DDPF_ALPHAPIXELS = 0x00000001u;

    constexpr uint32_t DDSCAPS2_CUBEMAP = 0x00000200u;
    constexpr uint32_t DDSCAPS2_CUBEMAP_ALLFACES =
        0x00000400u | // POSITIVEX
        0x00000800u | // NEGATIVEX
        0x00001000u | // POSITIVEY
        0x00002000u | // NEGATIVEY
        0x00004000u | // POSITIVEZ
        0x00008000u;  // NEGATIVEZ

    constexpr uint32_t DDS_RESOURCE_MISC_TEXTURECUBE = 0x4u;

    struct DDS_PIXELFORMAT {
        uint32_t size;
        uint32_t flags;
        uint32_t fourCC;
        uint32_t RGBBitCount;
        uint32_t RBitMask;
        uint32_t GBitMask;
        uint32_t BBitMask;
        uint32_t ABitMask;
    };
    static_assert(sizeof(DDS_PIXELFORMAT) == 32, "DDS_PIXELFORMAT size mismatch");

    struct DDS_HEADER {
        uint32_t size;
        uint32_t flags;
        uint32_t height;
        uint32_t width;
        uint32_t pitchOrLinearSize;
        uint32_t depth;
        uint32_t mipMapCount;
        uint32_t reserved1[11];
        DDS_PIXELFORMAT ddspf;
        uint32_t caps;
        uint32_t caps2;
        uint32_t caps3;
        uint32_t caps4;
        uint32_t reserved2;
    };
    static_assert(sizeof(DDS_HEADER) == 124, "DDS_HEADER size mismatch");

    struct DDS_HEADER_DXT10 {
        uint32_t dxgiFormat;
        uint32_t resourceDimension;
        uint32_t miscFlag;
        uint32_t arraySize;
        uint32_t miscFlags2;
    };
    static_assert(sizeof(DDS_HEADER_DXT10) == 20, "DDS_HEADER_DXT10 size mismatch");

    bool IsBCFormat(DXGI_FORMAT fmt) {
        switch (fmt) {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return true;
        default:
            return false;
        }
    }

    uint32_t BytesPerBlock(DXGI_FORMAT fmt) {
        switch (fmt) {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return 8;
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return 16;
        default:
            return 0;
        }
    }

    bool IsRGBA8Legacy(const DDS_PIXELFORMAT& pf, DXGI_FORMAT& outFmt) {
        if ((pf.flags & DDPF_RGB) == 0) return false;
        if (pf.RGBBitCount != 32) return false;

        // Common legacy layouts (Windows DDS): BGRA/RGBA variations.
        // Classic RGBA8: R=0x00ff0000, G=0x0000ff00, B=0x000000ff, A=0xff000000
        const bool hasAlpha = (pf.flags & DDPF_ALPHAPIXELS) != 0;
        if (pf.RBitMask == 0x00ff0000u &&
            pf.GBitMask == 0x0000ff00u &&
            pf.BBitMask == 0x000000ffu &&
            (!hasAlpha || pf.ABitMask == 0xff000000u)) {
            outFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
            return true;
        }

        // BGRA8: B=0x00ff0000, G=0x0000ff00, R=0x000000ff, A=0xff000000
        if (pf.BBitMask == 0x00ff0000u &&
            pf.GBitMask == 0x0000ff00u &&
            pf.RBitMask == 0x000000ffu &&
            (!hasAlpha || pf.ABitMask == 0xff000000u)) {
            outFmt = DXGI_FORMAT_B8G8R8A8_UNORM;
            return true;
        }

        return false;
    }

    bool ReadWholeFile(const wchar_t* path, std::vector<uint8_t>& outData, std::string* outError) {
        HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            if (outError) *outError = "Failed to open DDS file (check path/working directory).";
            return false;
        }

        LARGE_INTEGER sz = {};
        if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0) {
            CloseHandle(h);
            if (outError) *outError = "Invalid DDS file size.";
            return false;
        }
        

        outData.resize((size_t)sz.QuadPart);
        size_t readTotal = 0;
        while (readTotal < outData.size()) {
            DWORD toRead = (DWORD)std::min<size_t>(outData.size() - readTotal, 16 * 1024 * 1024);
            DWORD readNow = 0;
            if (!ReadFile(h, outData.data() + readTotal, toRead, &readNow, nullptr) || readNow == 0) {
                CloseHandle(h);
                if (outError) *outError = "Error reading DDS file.";
                return false;
            }
            readTotal += readNow;
        }
        CloseHandle(h);
        return true;
    }

} 

bool LoadDDSFromFile(const wchar_t* path, DDSLoadedImage& out, std::string* outError) {
    out = {};

    std::vector<uint8_t> file;
    if (!ReadWholeFile(path, file, outError)) return false;

    if (file.size() < 4 + sizeof(DDS_HEADER)) {
        if (outError) *outError = "DDS file is too small.";
        return false;
    }

    const uint32_t magic = *(const uint32_t*)file.data();
    if (magic != DDS_MAGIC) {
        if (outError) *outError = "Invalid DDS magic (expected 'DDS ').";
        return false;
    }

    const DDS_HEADER* hdr = (const DDS_HEADER*)(file.data() + 4);
    if (hdr->size != 124 || hdr->ddspf.size != 32) {
        if (outError) *outError = "Invalid DDS header size.";
        return false;
    }

    out.width = hdr->width;
    out.height = hdr->height;
    out.mipCount = hdr->mipMapCount ? hdr->mipMapCount : 1;
    out.arraySize = 1;
    out.isCubemap = false;

    size_t dataOffset = 4 + sizeof(DDS_HEADER);

    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    bool hasDX10 = false;

    if ((hdr->ddspf.flags & DDPF_FOURCC) != 0 && hdr->ddspf.fourCC == MakeFourCC('D', 'X', '1', '0')) {
        hasDX10 = true;
        if (file.size() < dataOffset + sizeof(DDS_HEADER_DXT10)) {
            if (outError) *outError = "DDS DX10: file truncated (missing DXT10 header).";
            return false;
        }
        const DDS_HEADER_DXT10* dx10 = (const DDS_HEADER_DXT10*)(file.data() + dataOffset);
        dataOffset += sizeof(DDS_HEADER_DXT10);

        fmt = (DXGI_FORMAT)dx10->dxgiFormat;
        out.isCubemap = (dx10->miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE) != 0;

        if (dx10->resourceDimension != 3) {
            if (outError) *outError = "DDS DX10: only Texture2D resources are supported.";
            return false;
        }

        if (out.isCubemap) {
            const uint32_t cubeCount = dx10->arraySize ? dx10->arraySize : 1;
            if (cubeCount != 1) {
                if (outError) *outError = "DDS cubemap (DX10): only single cube supported (arraySize=1).";
                return false;
            }
            out.arraySize = 6;
        }
        else {
            out.arraySize = dx10->arraySize ? dx10->arraySize : 1;
        }
    }
    else if ((hdr->ddspf.flags & DDPF_FOURCC) != 0) {
        const uint32_t cc = hdr->ddspf.fourCC;
        if (cc == MakeFourCC('D', 'X', 'T', '1')) fmt = DXGI_FORMAT_BC1_UNORM;
        else if (cc == MakeFourCC('D', 'X', 'T', '3')) fmt = DXGI_FORMAT_BC2_UNORM;
        else if (cc == MakeFourCC('D', 'X', 'T', '5')) fmt = DXGI_FORMAT_BC3_UNORM;
        else if (cc == MakeFourCC('A', 'T', 'I', '1')) fmt = DXGI_FORMAT_BC4_UNORM;
        else if (cc == MakeFourCC('A', 'T', 'I', '2')) fmt = DXGI_FORMAT_BC5_UNORM;
        else {
            if (outError) *outError = "DDS: unsupported FOURCC (requires DXT1/DXT3/DXT5/ATI1/ATI2 or DX10).";
            return false;
        }

        if ((hdr->caps2 & DDSCAPS2_CUBEMAP) != 0) {
            out.isCubemap = true;
            if ((hdr->caps2 & DDSCAPS2_CUBEMAP_ALLFACES) != DDSCAPS2_CUBEMAP_ALLFACES) {
                if (outError) *outError = "DDS cubemap: data for all 6 faces is missing.";
                return false;
            }
            out.arraySize = 6;
        }
    }
    else {
        if (!IsRGBA8Legacy(hdr->ddspf, fmt)) {
            if (outError) *outError = "DDS: unsupported uncompressed format (only RGBA8/BGRA8 supported).";
            return false;
        }
    }

    out.format = fmt;

    if (out.width == 0 || out.height == 0) {
        if (outError) *outError = "DDS: width or height is 0.";
        return false;
    }
    if (out.mipCount == 0) out.mipCount = 1;

    const uint32_t totalSubresources = out.mipCount * out.arraySize;
    out.subresources.clear();
    out.subresources.reserve(totalSubresources);

    if (dataOffset >= file.size()) {
        if (outError) *outError = "DDS: image data is missing.";
        return false;
    }
    out.data.assign(file.begin() + dataOffset, file.end());

    size_t cur = 0;

    for (uint32_t arraySlice = 0; arraySlice < out.arraySize; ++arraySlice) {
        uint32_t w = out.width;
        uint32_t h = out.height;

        for (uint32_t mip = 0; mip < out.mipCount; ++mip) {
            uint32_t rowPitch = 0;
            uint32_t slicePitch = 0;

            if (IsBCFormat(out.format)) {
                const uint32_t bpb = BytesPerBlock(out.format);
                if (bpb == 0) {
                    if (outError) *outError = "DDS: unknown BC format for pitch calculation.";
                    return false;
                }
                const uint32_t blocksWide = std::max(1u, (w + 3u) / 4u);
                const uint32_t blocksHigh = std::max(1u, (h + 3u) / 4u);
                rowPitch = blocksWide * bpb;
                slicePitch = rowPitch * blocksHigh;
            }
            else {
               
                rowPitch = w * 4u;
                slicePitch = rowPitch * h;
            }

            if (cur + slicePitch > out.data.size()) {
                if (outError) *outError = "DDS: file truncated (insufficient data for mip/slice).";
                return false;
            }

            DDSLoadedImageSubresource sr;
            sr.dataOffset = cur;
            sr.rowPitch = rowPitch;
            sr.slicePitch = slicePitch;
            out.subresources.push_back(sr);

            cur += slicePitch;
            w = std::max(1u, w / 2u);
            h = std::max(1u, h / 2u);
        }
    }

    return true;
}