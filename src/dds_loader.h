#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <cstdint>
#include <string>
#include <vector>

struct DDSLoadedImageSubresource {
    size_t dataOffset = 0;
    uint32_t rowPitch = 0;
    uint32_t slicePitch = 0;
};

struct DDSLoadedImage {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipCount = 0;
    uint32_t arraySize = 1;
    bool isCubemap = false;

    std::vector<uint8_t> data;
    std::vector<DDSLoadedImageSubresource> subresources;
};

bool LoadDDSFromFile(const wchar_t* path, DDSLoadedImage& out, std::string* outError);

