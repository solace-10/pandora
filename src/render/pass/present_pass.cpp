#include "render/pass/present_pass.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "pandora.hpp"
#include "render/lighting/lighting_system.hpp"
#include "render/rendersystem.hpp"
#include "render/window.hpp"
#include "resources/resource_shader.hpp"
#include "resources/resource_system.hpp"

namespace WingsOfSteel
{

static constexpr uint32_t kBloomLevelCount = 6;
static const std::string kPresentShaderPath("/shaders/pandora/present.wgsl");
static const std::string kBloomShaderPath("/shaders/pandora/bloom.wgsl");

PresentPass::PresentPass()
    : Pass("Present pass")
{
}

void PresentPass::Execute(wgpu::CommandEncoder& encoder)
{
    if (!m_Pipeline)
    {
        RequestShaders();
        ClearSurface(encoder);
        return;
    }

    UpdateBloomTexture();

    const LightingSystem* pLightingSystem = GetRenderSystem()->GetLightingSystem();
    const float bloomStrength = pLightingSystem->GetBloomStrength();
    if (bloomStrength > 0.0f)
    {
        RenderBloom(encoder);
    }

    const struct
    {
        float exposure;
        float bloomStrength;
        float bloomScale;
        Tonemapper tonemapper;
    } settings{
        std::exp2(pLightingSystem->GetExposure()),
        bloomStrength,
        1.0f / static_cast<float>(m_BloomLevelViews.size()),
        pLightingSystem->GetTonemapper()
    };
    GetRenderSystem()->GetDevice().GetQueue().WriteBuffer(m_SettingsBuffer, 0, &settings, sizeof(settings));

    wgpu::SurfaceTexture surfaceTexture;
    GetWindow()->GetSurface().GetCurrentTexture(&surfaceTexture);

    // Rebuilt every frame, as resizing the window replaces the textures.
    std::array<wgpu::BindGroupEntry, 5> bindGroupEntries{ {
        { .binding = 0, .textureView = GetWindow()->GetSceneColorTexture().GetTextureView() },
        { .binding = 1, .buffer = m_SettingsBuffer, .size = sizeof(settings) },
        { .binding = 2, .textureView = GetWindow()->GetOverlayColorTexture().GetTextureView() },
        { .binding = 3, .textureView = m_BloomLevelViews[0] },
        { .binding = 4, .sampler = m_Sampler },
    } };

    wgpu::BindGroupDescriptor bindGroupDescriptor{
        .layout = m_Pipeline.GetBindGroupLayout(0),
        .entryCount = bindGroupEntries.size(),
        .entries = bindGroupEntries.data()
    };
    wgpu::BindGroup bindGroup = GetRenderSystem()->GetDevice().CreateBindGroup(&bindGroupDescriptor);

    wgpu::RenderPassColorAttachment colorAttachment{
        .view = surfaceTexture.texture.CreateView(),
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = wgpu::Color{ 0.0, 0.0, 0.0, 1.0 }
    };

    wgpu::RenderPassDescriptor renderPassDescriptor{
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment
    };

    wgpu::RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDescriptor);
    renderPass.SetPipeline(m_Pipeline);
    renderPass.SetBindGroup(0, bindGroup);
    renderPass.Draw(3);
    renderPass.End();
}

// Requested on the first frame rather than in the constructor, as the engine's default passes are
// constructed before the resource system.
void PresentPass::RequestShaders()
{
    if (m_ShadersRequested)
    {
        return;
    }
    m_ShadersRequested = true;

    GetResourceSystem()->RequestResources({ kPresentShaderPath, kBloomShaderPath }, [this](const std::unordered_map<std::string, ResourceSharedPtr>& resources) {
        m_pPresentShader = std::dynamic_pointer_cast<ResourceShader>(resources.at(kPresentShaderPath));
        m_pBloomShader = std::dynamic_pointer_cast<ResourceShader>(resources.at(kBloomShaderPath));
        CreateRenderPipelines();
    });
}

// Until the shaders have loaded there is nothing to present, but the passes after this one draw
// onto the surface and expect it to hold something.
void PresentPass::ClearSurface(wgpu::CommandEncoder& encoder)
{
    wgpu::SurfaceTexture surfaceTexture;
    GetWindow()->GetSurface().GetCurrentTexture(&surfaceTexture);

    wgpu::RenderPassColorAttachment colorAttachment{
        .view = surfaceTexture.texture.CreateView(),
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = wgpu::Color{ 0.0, 0.0, 0.0, 1.0 }
    };

    wgpu::RenderPassDescriptor renderPassDescriptor{
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment
    };

    encoder.BeginRenderPass(&renderPassDescriptor).End();
}

void PresentPass::CreateRenderPipelines()
{
    wgpu::Device device = GetRenderSystem()->GetDevice();

    auto createPipeline = [&device](const char* pLabel, const wgpu::ShaderModule& module, const char* pEntryPoint, wgpu::TextureFormat format, const wgpu::BlendState* pBlendState = nullptr) {
        wgpu::ColorTargetState colorTargetState{
            .format = format,
            .blend = pBlendState
        };

        wgpu::FragmentState fragmentState{
            .module = module,
            .entryPoint = pEntryPoint,
            .targetCount = 1,
            .targets = &colorTargetState
        };

        wgpu::RenderPipelineDescriptor descriptor{
            .label = pLabel,
            .vertex = {
                .module = module,
                .entryPoint = "vertexMain" },
            .fragment = &fragmentState
        };
        return device.CreateRenderPipeline(&descriptor);
    };

    const wgpu::ShaderModule presentModule = m_pPresentShader->GetShaderModule();
    m_Pipeline = createPipeline("Present pipeline", presentModule, "presentMain", GetWindow()->GetSurfaceFormat());

    const wgpu::ShaderModule bloomModule = m_pBloomShader->GetShaderModule();
    const wgpu::TextureFormat bloomFormat = GetWindow()->GetSceneFormat();
    const wgpu::BlendState additive{
        .color = { .operation = wgpu::BlendOperation::Add, .srcFactor = wgpu::BlendFactor::One, .dstFactor = wgpu::BlendFactor::One },
        .alpha = { .operation = wgpu::BlendOperation::Add, .srcFactor = wgpu::BlendFactor::One, .dstFactor = wgpu::BlendFactor::One }
    };
    m_DownsamplePipeline = createPipeline("Bloom downsample pipeline", bloomModule, "downsampleMain", bloomFormat);
    m_UpsamplePipeline = createPipeline("Bloom upsample pipeline", bloomModule, "upsampleMain", bloomFormat, &additive);

    wgpu::SamplerDescriptor samplerDescriptor{
        .magFilter = wgpu::FilterMode::Linear,
        .minFilter = wgpu::FilterMode::Linear
    };
    m_Sampler = device.CreateSampler(&samplerDescriptor);

    wgpu::BufferDescriptor bufferDescriptor{
        .label = "Present settings buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
        .size = 4 * sizeof(float)
    };
    m_SettingsBuffer = device.CreateBuffer(&bufferDescriptor);
}

void PresentPass::UpdateBloomTexture()
{
    const uint32_t width = std::max(1u, GetWindow()->GetWidth() / 2);
    const uint32_t height = std::max(1u, GetWindow()->GetHeight() / 2);
    if (m_BloomTexture && m_BloomTexture.GetWidth() == width && m_BloomTexture.GetHeight() == height)
    {
        return;
    }

    const uint32_t levelCount = std::min(kBloomLevelCount, static_cast<uint32_t>(std::log2(std::max(width, height))) + 1);
    wgpu::TextureDescriptor textureDescriptor{
        .label = "Bloom texture",
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
        .size = { .width = width, .height = height },
        .format = GetWindow()->GetSceneFormat(),
        .mipLevelCount = levelCount
    };
    m_BloomTexture = GetRenderSystem()->GetDevice().CreateTexture(&textureDescriptor);

    m_BloomLevelViews.clear();
    for (uint32_t level = 0; level < levelCount; level++)
    {
        wgpu::TextureViewDescriptor viewDescriptor{
            .baseMipLevel = level,
            .mipLevelCount = 1
        };
        m_BloomLevelViews.push_back(m_BloomTexture.CreateView(&viewDescriptor));
    }
}

void PresentPass::RenderBloom(wgpu::CommandEncoder& encoder)
{
    const size_t levelCount = m_BloomLevelViews.size();

    DrawFullscreen(encoder, m_DownsamplePipeline, m_BloomLevelViews[0], wgpu::LoadOp::Clear, GetWindow()->GetSceneColorTexture().GetTextureView());
    for (size_t level = 1; level < levelCount; level++)
    {
        DrawFullscreen(encoder, m_DownsamplePipeline, m_BloomLevelViews[level], wgpu::LoadOp::Clear, m_BloomLevelViews[level - 1]);
    }

    for (size_t level = levelCount - 1; level > 0; level--)
    {
        DrawFullscreen(encoder, m_UpsamplePipeline, m_BloomLevelViews[level - 1], wgpu::LoadOp::Load, m_BloomLevelViews[level]);
    }
}

void PresentPass::DrawFullscreen(wgpu::CommandEncoder& encoder, const wgpu::RenderPipeline& pipeline, const wgpu::TextureView& target, wgpu::LoadOp loadOp, const wgpu::TextureView& source)
{
    std::array<wgpu::BindGroupEntry, 2> bindGroupEntries{ {
        { .binding = 0, .textureView = source },
        { .binding = 1, .sampler = m_Sampler },
    } };

    wgpu::BindGroupDescriptor bindGroupDescriptor{
        .layout = pipeline.GetBindGroupLayout(0),
        .entryCount = bindGroupEntries.size(),
        .entries = bindGroupEntries.data()
    };
    wgpu::BindGroup bindGroup = GetRenderSystem()->GetDevice().CreateBindGroup(&bindGroupDescriptor);

    wgpu::RenderPassColorAttachment colorAttachment{
        .view = target,
        .loadOp = loadOp,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = wgpu::Color{ 0.0, 0.0, 0.0, 0.0 }
    };

    wgpu::RenderPassDescriptor renderPassDescriptor{
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment
    };

    wgpu::RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDescriptor);
    renderPass.SetPipeline(pipeline);
    renderPass.SetBindGroup(0, bindGroup);
    renderPass.Draw(3);
    renderPass.End();
}

} // namespace WingsOfSteel
