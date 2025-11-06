#pragma once

#include <webgpu/webgpu_cpp.h>

#include "resources/resource_shader.hpp"
#include "scene/components/landscape_component.hpp"
#include "scene/systems/system.hpp"

namespace WingsOfSteel
{

class LandscapeRenderSystem : public System
{
public:
    LandscapeRenderSystem();
    ~LandscapeRenderSystem();

    void Initialize(Scene* pScene) override{};
    void Update(float delta) override;

    void Render(wgpu::RenderPassEncoder& renderPass);

private:
    void GenerateGeometry(const LandscapeComponent& landscapeComponent);
    void CreateRenderPipeline();

    ResourceShaderSharedPtr m_pShader;
    wgpu::RenderPipeline m_RenderPipeline;
    wgpu::Buffer m_VertexBuffer;
    wgpu::Buffer m_IndexBuffer;
    uint32_t m_VertexCount{ 0 };
    uint32_t m_IndexCount{ 0 };
    uint32_t m_Generation{ 0 };
    bool m_Initialized{ false };
};

} // namespace WingsOfSteel
