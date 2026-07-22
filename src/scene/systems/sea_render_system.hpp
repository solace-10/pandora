#pragma once

#include <optional>

#include <glm/vec4.hpp>
#include <webgpu/webgpu_cpp.h>

#include "core/signal.hpp"
#include "imgui/idebugui.hpp"
#include "resources/resource_shader.hpp"
#include "scene/components/landscape_component.hpp"
#include "scene/components/sea_component.hpp"
#include "scene/systems/system.hpp"

namespace WingsOfSteel
{

class SeaRenderSystem : public System, public IDebugUI
{
public:
    SeaRenderSystem();
    ~SeaRenderSystem();

    void Initialize(Scene* pScene) override{};
    void Update(float delta) override;

    void Render(wgpu::RenderPassEncoder& renderPass);

    void DrawDebugUI() override;

private:
    void GenerateGeometry(const SeaComponent& seaComponent, const LandscapeComponent* pLandscapeComponent);
    void CreateUniforms();
    void UpdateUniforms(const SeaComponent& seaComponent);
    void CreateRenderPipeline();
    void HandleShaderInjection();

    // Mirrored in sea.wgsl; everything is packed into vec4s to sidestep alignment pitfalls.
    struct SeaUniforms
    {
        glm::vec4 waves[3]; // xy: direction, z: steepness, w: wavelength
        glm::vec4 shallowColor;
        glm::vec4 deepColor;
        glm::vec4 params0; // x: depth falloff, y: foam depth, z: foam noise scale, w: foam speed
        glm::vec4 params1; // x: specular strength, y: fresnel strength, z: sparkle strength, w: sparkle scale
        glm::vec4 params2; // x: amplitude, y: speed
    };

    ResourceShaderSharedPtr m_pShader;
    wgpu::RenderPipeline m_RenderPipeline;
    wgpu::Buffer m_VertexBuffer;
    wgpu::Buffer m_IndexBuffer;
    uint32_t m_IndexCount{ 0 };
    uint32_t m_Generation{ 0 };
    uint32_t m_LandscapeGeneration{ 0 };
    SeaUniforms m_Uniforms;
    wgpu::Buffer m_UniformsBuffer;
    wgpu::BindGroupLayout m_UniformsBindGroupLayout;
    wgpu::BindGroup m_UniformsBindGroup;
    bool m_Initialized{ false };
    std::optional<SignalId> m_ShaderInjectionSignalId;
};

} // namespace WingsOfSteel
