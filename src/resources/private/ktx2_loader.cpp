#include "resources/private/ktx2_loader.hpp"

#include <algorithm>
#include <cstring>

#include <ktx.h>
#include <magic_enum.hpp>

#include "core/log.hpp"
#include "pandora.hpp"
#include "render/rendersystem.hpp"
#include "render/texture_format.hpp"

namespace WingsOfSteel
{

// Owns the ktxTexture2 the level spans point into.
struct Ktx2Image::Payload
{
    ~Payload()
    {
        if (pTexture)
        {
            ktxTexture2_Destroy(pTexture);
        }
    }

    ktxTexture2* pTexture{ nullptr };
};

Ktx2Image::Ktx2Image() = default;
Ktx2Image::~Ktx2Image() = default;
Ktx2Image::Ktx2Image(Ktx2Image&&) noexcept = default;
Ktx2Image& Ktx2Image::operator=(Ktx2Image&&) noexcept = default;

bool IsKtx2(const uint8_t* pData, size_t dataSize)
{
    static const uint8_t identifier[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32,
        0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
    return pData != nullptr && dataSize >= sizeof(identifier) && memcmp(pData, identifier, sizeof(identifier)) == 0;
}

namespace
{

    // Picks what a Basis Universal payload should become on this device.
    //
    // The order matters: BC7 and ASTC keep far more quality than the ETC2 and BC1/BC3
    // fallbacks, and RGBA32 is a last resort that gives up compression entirely rather
    // than failing to display the texture at all.
    ktx_transcode_fmt_e SelectTranscodeFormat()
    {
        RenderSystem* pRenderSystem = GetRenderSystem();

        if (pRenderSystem->HasFeature(wgpu::FeatureName::TextureCompressionBC))
        {
            return KTX_TTF_BC7_RGBA;
        }
        else if (pRenderSystem->HasFeature(wgpu::FeatureName::TextureCompressionASTC))
        {
            return KTX_TTF_ASTC_4x4_RGBA;
        }
        else if (pRenderSystem->HasFeature(wgpu::FeatureName::TextureCompressionETC2))
        {
            return KTX_TTF_ETC2_RGBA;
        }
        else
        {
            return KTX_TTF_RGBA32;
        }
    }

} // namespace

std::optional<Ktx2Image> LoadKtx2(const std::string& label, const uint8_t* pData, size_t dataSize)
{
    ktxTexture2* pTexture = nullptr;

    // LOAD_IMAGE_DATA_BIT reads the levels into memory and, if the file is
    // supercompressed, inflates them.
    KTX_error_code result = ktxTexture2_CreateFromMemory(
        pData,
        dataSize,
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &pTexture);

    if (result != KTX_SUCCESS)
    {
        Log::Error() << "Failed to read KTX2 texture '" << label << "': " << ktxErrorString(result);
        return std::nullopt;
    }

    Ktx2Image image;
    image.m_pPayload = std::make_unique<Ktx2Image::Payload>();
    image.m_pPayload->pTexture = pTexture;

    image.m_Width = pTexture->baseWidth;
    image.m_Height = pTexture->baseHeight;

    if (ktxTexture2_NeedsTranscoding(pTexture))
    {
        const ktx_transcode_fmt_e transcodeFormat = SelectTranscodeFormat();

        // WebGPU requires a compressed texture's dimensions to be a multiple of the
        // block size, which a transcode cannot fix. Falling back to uncompressed keeps
        // an oddly sized texture displaying rather than failing device validation.
        const bool isBlockFormat = (transcodeFormat != KTX_TTF_RGBA32);
        if (isBlockFormat && (image.m_Width % 4 != 0 || image.m_Height % 4 != 0))
        {
            Log::Warning() << "KTX2 texture '" << label << "' is " << image.m_Width << "x" << image.m_Height
                           << ", which is not a multiple of the 4x4 block size. Falling back to uncompressed.";
            result = ktxTexture2_TranscodeBasis(pTexture, KTX_TTF_RGBA32, 0);
        }
        else
        {
            result = ktxTexture2_TranscodeBasis(pTexture, transcodeFormat, 0);
        }

        if (result != KTX_SUCCESS)
        {
            Log::Error() << "Failed to transcode KTX2 texture '" << label << "': " << ktxErrorString(result);
            return std::nullopt;
        }
    }

    image.m_Format = VkFormatToTextureFormat(pTexture->vkFormat);
    if (image.m_Format == wgpu::TextureFormat::Undefined)
    {
        Log::Error() << "KTX2 texture '" << label << "' has VkFormat " << pTexture->vkFormat
                     << ", which has no WebGPU equivalent.";
        return std::nullopt;
    }

    const uint8_t* pLevelData = ktxTexture_GetData(ktxTexture(pTexture));
    const size_t totalSize = ktxTexture_GetDataSize(ktxTexture(pTexture));

    image.m_Levels.reserve(pTexture->numLevels);
    for (uint32_t level = 0; level < pTexture->numLevels; level++)
    {
        ktx_size_t offset = 0;
        result = ktxTexture_GetImageOffset(ktxTexture(pTexture), level, 0, 0, &offset);
        if (result != KTX_SUCCESS)
        {
            Log::Error() << "Failed to locate mip level " << level << " of KTX2 texture '" << label
                         << "': " << ktxErrorString(result);
            return std::nullopt;
        }

        const size_t levelSize = ktxTexture_GetImageSize(ktxTexture(pTexture), level);
        if (offset + levelSize > totalSize)
        {
            Log::Error() << "Mip level " << level << " of KTX2 texture '" << label << "' runs past the end of the file.";
            return std::nullopt;
        }

        image.m_Levels.emplace_back(pLevelData + offset, levelSize);
    }

    Log::Info() << "Loaded KTX2 texture '" << label << "': " << image.m_Width << "x" << image.m_Height
                << ", " << image.m_Levels.size() << " mip levels, " << magic_enum::enum_name(image.m_Format) << ".";

    return image;
}

} // namespace WingsOfSteel
