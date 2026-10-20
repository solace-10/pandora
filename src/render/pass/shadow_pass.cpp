#include "render/pass/shadow_pass.hpp"

#include "pandora.hpp"
#include "render/rendersystem.hpp"
#include "resources/resource_shader.hpp"
#include "resources/resource_system.hpp"
#include "scene/scene.hpp"
#include "scene/systems/model_render_system.hpp"

namespace WingsOfSteel
{

static constexpr uint32_t kDebugViewSize = 512;

ShadowPass::ShadowPass()
    : Pass("Shadow pass")
{
}

void ShadowPass::Execute(wgpu::CommandEncoder& encoder)
{
    // Cleared even without a scene, so that nothing reads last frame's shadows.
    wgpu::RenderPassDepthStencilAttachment depthAttachment{
        .view = GetRenderSystem()->GetShadowMapView(),
        .depthLoadOp = wgpu::LoadOp::Clear,
        .depthStoreOp = wgpu::StoreOp::Store,
        .depthClearValue = 1.0f
    };

    wgpu::RenderPassDescriptor renderpass{
        .label = "Shadow pass",
        .colorAttachmentCount = 0,
        .depthStencilAttachment = &depthAttachment
    };

    wgpu::RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderpass);
    GetRenderSystem()->BindShadowGlobalUniforms(renderPass);

    Scene* pScene = GetActiveScene();
    if (pScene)
    {
        ModelRenderSystem* pModelRenderSystem = pScene->GetSystem<ModelRenderSystem>();
        if (pModelRenderSystem)
        {
            pModelRenderSystem->RenderShadows(renderPass);
        }
    }

    renderPass.End();

    if (m_DebugViewRequested)
    {
        RenderDebugView(encoder);
        m_DebugViewRequested = false;
    }
}

wgpu::TextureView& ShadowPass::RequestDebugView()
{
    m_DebugViewRequested = true;
    if (m_DebugTexture)
    {
        return m_DebugTextureView;
    }

    wgpu::TextureDescriptor textureDescriptor{
        .label = "Shadow map debug view",
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
        .size{ .width = kDebugViewSize, .height = kDebugViewSize },
        .format = wgpu::TextureFormat::RGBA8Unorm
    };
    m_DebugTexture = GetRenderSystem()->GetDevice().CreateTexture(&textureDescriptor);
    m_DebugTextureView = m_DebugTexture.CreateView();

    GetResourceSystem()->RequestResource("/shaders/pandora/shadow_map_debug.wgsl", [this](ResourceSharedPtr pResource) {
        m_pDebugShader = std::dynamic_pointer_cast<ResourceShader>(pResource);
        wgpu::ColorTargetState colorTargetState{ .format = wgpu::TextureFormat::RGBA8Unorm };
        wgpu::FragmentState fragmentState{
            .module = m_pDebugShader->GetShaderModule(),
            .targetCount = 1,
            .targets = &colorTargetState
        };
        wgpu::RenderPipelineDescriptor descriptor{
            .label = "Shadow map debug view",
            .vertex = { .module = m_pDebugShader->GetShaderModule() },
            .fragment = &fragmentState
        };
        m_DebugPipeline = GetRenderSystem()->GetDevice().CreateRenderPipeline(&descriptor);

        wgpu::BindGroupEntry bindGroupEntry{
            .binding = 0,
            .textureView = GetRenderSystem()->GetShadowMapView()
        };
        wgpu::BindGroupDescriptor bindGroupDescriptor{
            .label = "Shadow map debug view",
            .layout = m_DebugPipeline.GetBindGroupLayout(0),
            .entryCount = 1,
            .entries = &bindGroupEntry
        };
        m_DebugBindGroup = GetRenderSystem()->GetDevice().CreateBindGroup(&bindGroupDescriptor);
    });

    return m_DebugTextureView;
}

void ShadowPass::RenderDebugView(wgpu::CommandEncoder& encoder)
{
    if (!m_DebugPipeline)
    {
        return;
    }

    wgpu::RenderPassColorAttachment colorAttachment{
        .view = m_DebugTextureView,
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = wgpu::Color{ 0.0, 0.0, 0.0, 1.0 }
    };

    wgpu::RenderPassDescriptor renderpass{
        .label = "Shadow map debug view",
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment
    };

    wgpu::RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderpass);
    renderPass.SetPipeline(m_DebugPipeline);
    renderPass.SetBindGroup(0, m_DebugBindGroup);
    renderPass.Draw(3);
    renderPass.End();
}

} // namespace WingsOfSteel
