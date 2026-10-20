#pragma once

#include "render/pass/pass.hpp"

namespace WingsOfSteel
{

DECLARE_SMART_PTR(ResourceShader);

// Renders the opaque models into the directional light's shadow map, which the scene's shaders then
// sample through shadows.wgsl. Must run before BaseRenderPass.
DECLARE_SMART_PTR(ShadowPass);
class ShadowPass : public Pass
{
public:
    ShadowPass();

    void Execute(wgpu::CommandEncoder& encoder) override;

    // The shadow map as greyscale, for ImGui. Only rendered on frames which request it, so call this
    // every frame it is shown, before the passes run.
    wgpu::TextureView& RequestDebugView();

private:
    void RenderDebugView(wgpu::CommandEncoder& encoder);

    bool m_DebugViewRequested{ false };
    ResourceShaderSharedPtr m_pDebugShader;
    wgpu::RenderPipeline m_DebugPipeline;
    wgpu::BindGroup m_DebugBindGroup;
    wgpu::Texture m_DebugTexture;
    wgpu::TextureView m_DebugTextureView;
};

} // namespace WingsOfSteel
