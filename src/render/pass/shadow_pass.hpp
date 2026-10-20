#pragma once

#include <glm/mat4x4.hpp>

#include "render/pass/pass.hpp"

namespace WingsOfSteel
{

DECLARE_SMART_PTR(ResourceShader);

// Renders the opaque models into a shadow map. By default that is the directional light's, from the
// LightingSystem's shadow view, which the scene's shaders then sample through shadows.wgsl; it must
// run before BaseRenderPass.
DECLARE_SMART_PTR(ShadowPass);
class ShadowPass : public Pass
{
public:
    ShadowPass();

    // Renders into another Depth32Float texture, from the view given to SetViewProjection(): shadows
    // for a light other than the scene's. Such a pass needn't be added to the RenderSystem; its owner
    // can call Execute() itself, when it needs the shadows.
    ShadowPass(const std::string& name, const wgpu::TextureView& target);

    void Execute(wgpu::CommandEncoder& encoder) override;

    // World to the target's clip space. Only for a pass with its own target.
    void SetViewProjection(const glm::mat4& viewProjection);

    // The shadow map as greyscale, for ImGui. Only rendered on frames which request it, so call this
    // every frame it is shown, before the passes run.
    wgpu::TextureView& RequestDebugView();

private:
    void RenderDebugView(wgpu::CommandEncoder& encoder);
    const wgpu::TextureView& GetTarget() const;

    wgpu::TextureView m_Target;
    glm::mat4 m_ViewProjection{ 1.0f };
    wgpu::Buffer m_ViewProjectionBuffer;
    wgpu::BindGroup m_ViewProjectionBindGroup;

    bool m_DebugViewRequested{ false };
    ResourceShaderSharedPtr m_pDebugShader;
    wgpu::RenderPipeline m_DebugPipeline;
    wgpu::BindGroup m_DebugBindGroup;
    wgpu::Texture m_DebugTexture;
    wgpu::TextureView m_DebugTextureView;
};

} // namespace WingsOfSteel
