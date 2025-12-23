#include "resources/resource_texture_2d.hpp"

#include "core/log.hpp"
#include "pandora.hpp"
#include "render/rendersystem.hpp"

// clang-format off
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG // Better error messages.
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#include <stb_image.h>
// clang-format on

namespace WingsOfSteel
{

ResourceTexture2D::ResourceTexture2D(const std::string& label, ColorSpace colorSpace, const unsigned char* pData, size_t dataSize)
{
    LoadFromMemoryCompressed(label, colorSpace, pData, dataSize);
}


ResourceTexture2D::ResourceTexture2D(const std::string& label, ColorSpace colorSpace, const unsigned char* pData, size_t dataSize, uint32_t width, uint32_t height, uint32_t channels)
{
    LoadFromMemoryUncompressed(label, colorSpace, pData, dataSize, width, height, channels);
}

ResourceTexture2D::~ResourceTexture2D()
{
    if (m_Texture)
    {
        m_Texture.Destroy();
    }
}

void ResourceTexture2D::Load(const std::string& path)
{
    Resource::Load(path);

    GetVFS()->FileRead(path,
        [this](FileReadResult result, FileSharedPtr pFile) {
            this->LoadInternal(result, pFile);
        });
}

ResourceType ResourceTexture2D::GetResourceType() const
{
    return ResourceType::Texture2D;
}

wgpu::TextureView ResourceTexture2D::GetTextureView() const
{
    return m_TextureView;
}

void ResourceTexture2D::LoadInternal(FileReadResult result, FileSharedPtr pFile)
{
    if (result == FileReadResult::Ok)
    {
        // We currently assume that all textures loaded through the VFS are in sRGB.
        // This will be true in most cases, but we really should check the file header for colorspace information.
        LoadFromMemoryCompressed(pFile->GetPath(), ColorSpace::sRGB, reinterpret_cast<const unsigned char*>(pFile->GetData().data()), pFile->GetData().size());
    }
    else
    {
        SetState(ResourceState::Error);
    }
}

void ResourceTexture2D::LoadFromMemoryCompressed(const std::string& label, ColorSpace colorSpace, const unsigned char* pData, size_t dataSize)
{
    m_Channels = 4; // Setting desired channels to 4 as WGPU has no RGB8, just RGBA8.
    int width = 0;
    int height = 0; 
    int channelsInMemory = 0;
    unsigned char* pTextureData = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(pData),
        dataSize,
        &width,
        &height,
        &channelsInMemory,
        m_Channels);

    m_Width = static_cast<uint32_t>(width);
    m_Height = static_cast<uint32_t>(height);
    const size_t textureDataSize = m_Width * m_Height * m_Channels;

    if (pTextureData)
    {
        wgpu::TextureDescriptor textureDescriptor{
            .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
            .dimension = wgpu::TextureDimension::e2D,
            .size = { m_Width, m_Height, 1 },
            .format = GetTextureFormat(colorSpace),
            .mipLevelCount = 1,
            .sampleCount = 1
        };

        m_Texture = GetRenderSystem()->GetDevice().CreateTexture(&textureDescriptor);

        wgpu::ImageCopyTexture destination{
            .texture = m_Texture,
            .aspect = wgpu::TextureAspect::All
        };

        wgpu::TextureDataLayout sourceLayout{
            .offset = 0,
            .bytesPerRow = m_Channels * m_Width,
            .rowsPerImage = m_Height
        };

        const size_t textureDataSize = m_Width * m_Height * m_Channels;
        GetRenderSystem()->GetDevice().GetQueue().WriteTexture(&destination, pTextureData, textureDataSize, &sourceLayout, &textureDescriptor.size);

        wgpu::TextureViewDescriptor textureViewDescriptor{
            .label = label.c_str(),
            .format = textureDescriptor.format,
            .dimension = wgpu::TextureViewDimension::e2D,
            .mipLevelCount = textureDescriptor.mipLevelCount,
            .arrayLayerCount = 1
        };
        m_TextureView = m_Texture.CreateView(&textureViewDescriptor);

        stbi_image_free(pTextureData);
        SetState(ResourceState::Loaded);
    }
    else
    {
        SetState(ResourceState::Error);
        Log::Error() << "Failed to load texture '" << label << "': " << stbi_failure_reason();
    }
}

void ResourceTexture2D::LoadFromMemoryUncompressed(const std::string& label, ColorSpace colorSpace, const unsigned char* pData, size_t dataSize, uint32_t width, uint32_t height, uint32_t channels)
{
    m_Width = width;
    m_Height = height;
    m_Channels = channels;
    wgpu::TextureDescriptor textureDescriptor{
        .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
        .dimension = wgpu::TextureDimension::e2D,
        .size = { m_Width, m_Height, 1 },
        .format = GetTextureFormat(colorSpace),
        .mipLevelCount = 1,
        .sampleCount = 1
    };

    m_Texture = GetRenderSystem()->GetDevice().CreateTexture(&textureDescriptor);

    wgpu::ImageCopyTexture destination{
        .texture = m_Texture,
        .aspect = wgpu::TextureAspect::All
    };

    wgpu::TextureDataLayout sourceLayout{
        .offset = 0,
        .bytesPerRow = m_Channels * m_Width,
        .rowsPerImage = m_Height
    };

    const size_t textureDataSize = m_Width * m_Height * m_Channels;
    GetRenderSystem()->GetDevice().GetQueue().WriteTexture(&destination, pData, dataSize, &sourceLayout, &textureDescriptor.size);

    wgpu::TextureViewDescriptor textureViewDescriptor{
        .label = label.c_str(),
        .format = textureDescriptor.format,
        .dimension = wgpu::TextureViewDimension::e2D,
        .mipLevelCount = textureDescriptor.mipLevelCount,
        .arrayLayerCount = 1
    };
    m_TextureView = m_Texture.CreateView(&textureViewDescriptor);

    SetState(ResourceState::Loaded);
}

wgpu::TextureFormat ResourceTexture2D::GetTextureFormat(ColorSpace colorSpace) const
{
    return (colorSpace == ColorSpace::Linear) ? wgpu::TextureFormat::RGBA8Unorm : wgpu::TextureFormat::RGBA8UnormSrgb;
}

} // namespace WingsOfSteel
