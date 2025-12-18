#pragma once
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <iostream>
#include <d3d12.h>
#include "Core.h"
#include <map>

class Texture
{
public:
    int width = 0;
    int height = 0;
    ID3D12Resource* tex = nullptr;
    int heapOffset = 0;
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    int channels = 4;

    void upload(unsigned int width, unsigned int height, const void* data, unsigned int alignedRowPitch, Core* core)
    {
        D3D12_HEAP_PROPERTIES heapDesc = {};
        heapDesc.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Width = width;
        textureDesc.Height = height;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.MipLevels = 1;
        textureDesc.Format = format;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        core->device->CreateCommittedResource(
            &heapDesc, D3D12_HEAP_FLAG_NONE, &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&tex));

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        footprint.Footprint.Format = format;
        footprint.Footprint.Width = width;
        footprint.Footprint.Height = height;
        footprint.Footprint.Depth = 1;
        footprint.Footprint.RowPitch = alignedRowPitch;

        unsigned long long totalSize = (unsigned long long)alignedRowPitch * height;

        core->uploadResource(tex, data, totalSize, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &footprint);

        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = core->srvHeap.getNextCPUHandle();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        core->device->CreateShaderResourceView(tex, &srvDesc, srvHandle);

        heapOffset = core->srvHeap.used - 1;
    }

    void load(const std::string& filename, Core* core)
    {
        int loadedChannels = 0;
        unsigned char* texels = stbi_load(filename.c_str(), &width, &height, &loadedChannels, 4);

        if (!texels) {
            std::cout << "ERROR: Failed to load texture: " << filename << ". Using fallback." << std::endl;

            width = 1;
            height = 1;
            unsigned int rowPitch = 4; 
            unsigned int alignedRowPitch = (rowPitch + 255) & ~255;
            unsigned char* fallbackData = new unsigned char[alignedRowPitch];
            fallbackData[0] = 255; fallbackData[1] = 0; fallbackData[2] = 255; fallbackData[3] = 255;

            upload(width, height, fallbackData, alignedRowPitch, core);
            delete[] fallbackData;
            return;
        }

        channels = 4;
        int bytesPerPixel = 4;

        unsigned int rowPitch = width * bytesPerPixel;
        unsigned int alignedRowPitch = (rowPitch + 255) & ~255;

        if (rowPitch == alignedRowPitch)
        {
            upload(width, height, texels, alignedRowPitch, core);
        }
        else
        {
            unsigned char* alignedData = new unsigned char[alignedRowPitch * height];
            for (int i = 0; i < height; i++)
            {
                memcpy(&alignedData[i * alignedRowPitch], &texels[i * rowPitch], rowPitch);
            }

            upload(width, height, alignedData, alignedRowPitch, core);
            delete[] alignedData;
        }

        stbi_image_free(texels);
    }
};

class TextureManager
{
public:
    std::map<std::string, Texture*> textures;

    Texture* getTexture(const std::string& filename, Core* core)
    {
        auto it = textures.find(filename);
        if (it != textures.end())
            return it->second;

        Texture* tex = new Texture();
        tex->load(filename, core);
        textures[filename] = tex;
        return tex;
    }

    int getHeapOffset(const std::string& filename, Core* core)
    {
        Texture* t = getTexture(filename, core);
        if (t) return t->heapOffset;
        return 0;
    }

    ~TextureManager() {
        for (auto& pair : textures) {
            if (pair.second) {
                if (pair.second->tex) pair.second->tex->Release();
                delete pair.second;
            }
        }
    }
};