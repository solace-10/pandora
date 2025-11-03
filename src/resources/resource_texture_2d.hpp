#pragma once

#include "resources/resource.hpp"

#include <webgpu/webgpu_cpp.h>

namespace WingsOfSteel
{

class ResourceTexture2D : public Resource
{
public:
    ResourceTexture2D() {}
    ResourceTexture2D(const std::string& label, const unsigned char* pData, size_t dataSize); // Construct from compressed data.
    ResourceTexture2D(const std::string& label, const unsigned char* pData, size_t dataSize, uint32_t width, uint32_t height, uint32_t channels); // Construct from uncompressed data.
    ~ResourceTexture2D() override;

    void Load(const std::string& path) override;
    ResourceType GetResourceType() const override;

    wgpu::TextureView GetTextureView() const;

    uint32_t GetWidth() const;
    uint32_t GetHeight() const;

private:
    void LoadInternal(FileReadResult result, FileSharedPtr pFile);
    void LoadFromMemoryCompressed(const std::string& label, const unsigned char* pData, size_t dataSize);
    void LoadFromMemoryUncompressed(const std::string& label, const unsigned char* pData, size_t dataSize, uint32_t width, uint32_t height, uint32_t channels);

    wgpu::Texture m_Texture;
    wgpu::TextureView m_TextureView;
    uint32_t m_Width{ 0 };
    uint32_t m_Height{ 0 };
    uint32_t m_Channels{ 0 };
};

inline uint32_t ResourceTexture2D::GetWidth() const
{
    return m_Width;
}

inline uint32_t ResourceTexture2D::GetHeight() const
{
    return m_Height;
}

} // namespace WingsOfSteel
