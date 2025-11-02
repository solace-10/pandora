#pragma once

#include <webgpu/webgpu_cpp.h>

#include "resources/resource_shader.hpp"
#include "scene/systems/system.hpp"

namespace WingsOfSteel
{

class LandscapeRenderSystem : public System
{
public:
    LandscapeRenderSystem();
    ~LandscapeRenderSystem();

    void Initialize(Scene* pScene) override{};
    void Update(float delta) override{};

    void Render(wgpu::RenderPassEncoder& renderPass);

private:
    void GenerateGridGeometry();
    void CreateRenderPipeline();

    ResourceShaderSharedPtr m_pShader;
    wgpu::RenderPipeline m_RenderPipeline;
    wgpu::Buffer m_VertexBuffer;
    uint32_t m_VertexCount;
    bool m_Initialized;
};

} // namespace WingsOfSteel