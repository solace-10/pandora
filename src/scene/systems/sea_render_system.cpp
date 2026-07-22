#include "scene/systems/sea_render_system.hpp"

#include <glm/glm.hpp>

#include "pandora.hpp"
#include "render/rendersystem.hpp"
#include "render/vertex_types.hpp"
#include "render/window.hpp"
#include "resources/resource_system.hpp"
#include "scene/components/sea_component.hpp"
#include "scene/scene.hpp"

#include <array>

namespace WingsOfSteel
{

SeaRenderSystem::SeaRenderSystem()
{
    GetResourceSystem()->RequestResource("/shaders/sea.wgsl", [this](ResourceSharedPtr pResource) {
        m_pShader = std::dynamic_pointer_cast<ResourceShader>(pResource);
        CreateRenderPipeline();
        HandleShaderInjection();
        m_Initialized = true;
    });
}

SeaRenderSystem::~SeaRenderSystem()
{
    if (GetResourceSystem() && m_ShaderInjectionSignalId.has_value())
    {
        GetResourceSystem()->GetShaderInjectedSignal().Disconnect(m_ShaderInjectionSignalId.value());
    }
}

void SeaRenderSystem::Update(float delta)
{
    if (GetActiveScene() == nullptr)
    {
        return;
    }

    entt::registry& registry = GetActiveScene()->GetRegistry();
    auto view = registry.view<const SeaComponent>();

    view.each([this](const auto entity, const SeaComponent& seaComponent) {
        if (seaComponent.Generation == this->m_Generation)
        {
            return;
        }

        GenerateGeometry(seaComponent);
    });
}

void SeaRenderSystem::GenerateGeometry(const SeaComponent& seaComponent)
{
    const float halfWidth = seaComponent.Width / 2.0f;
    const float halfLength = seaComponent.Length / 2.0f;
    const glm::vec4 color = seaComponent.Color;

    // A single quad at water level (y = 0), centered on the origin.
    const std::array<VertexP3C4UV, 4> vertices = { {
        { glm::vec3(-halfWidth, 0.0f, -halfLength), color, glm::vec2(0.0f, 0.0f) },
        { glm::vec3(halfWidth, 0.0f, -halfLength), color, glm::vec2(1.0f, 0.0f) },
        { glm::vec3(-halfWidth, 0.0f, halfLength), color, glm::vec2(0.0f, 1.0f) },
        { glm::vec3(halfWidth, 0.0f, halfLength), color, glm::vec2(1.0f, 1.0f) }
    } };

    const std::array<uint32_t, 6> indices = { 3, 1, 0, 2, 3, 0 };
    m_IndexCount = static_cast<uint32_t>(indices.size());

    wgpu::Device device = GetRenderSystem()->GetDevice();

    wgpu::BufferDescriptor vertexBufferDescriptor{
        .label = "Sea vertex buffer",
        .usage = wgpu::BufferUsage::Vertex,
        .size = vertices.size() * sizeof(VertexP3C4UV),
        .mappedAtCreation = true
    };
    m_VertexBuffer = device.CreateBuffer(&vertexBufferDescriptor);
    memcpy(m_VertexBuffer.GetMappedRange(), vertices.data(), vertices.size() * sizeof(VertexP3C4UV));
    m_VertexBuffer.Unmap();

    wgpu::BufferDescriptor indexBufferDescriptor{
        .label = "Sea index buffer",
        .usage = wgpu::BufferUsage::Index,
        .size = indices.size() * sizeof(uint32_t),
        .mappedAtCreation = true
    };
    m_IndexBuffer = device.CreateBuffer(&indexBufferDescriptor);
    memcpy(m_IndexBuffer.GetMappedRange(), indices.data(), indices.size() * sizeof(uint32_t));
    m_IndexBuffer.Unmap();

    m_Generation = seaComponent.Generation;
}

void SeaRenderSystem::CreateRenderPipeline()
{
    if (!m_pShader)
    {
        return;
    }

    wgpu::BlendState blendState{
        .color = {
            .operation = wgpu::BlendOperation::Add,
            .srcFactor = wgpu::BlendFactor::SrcAlpha,
            .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha },
        .alpha = { .operation = wgpu::BlendOperation::Add, .srcFactor = wgpu::BlendFactor::One, .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha }
    };

    wgpu::ColorTargetState colorTargetState{
        .format = GetWindow()->GetTextureFormat(),
        .blend = &blendState
    };

    wgpu::FragmentState fragmentState{
        .module = m_pShader->GetShaderModule(),
        .targetCount = 1,
        .targets = &colorTargetState
    };

    // Pipeline layout with global uniforms
    std::array<wgpu::BindGroupLayout, 1> bindGroupLayouts = {
        GetRenderSystem()->GetGlobalUniformsLayout()
    };
    wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor{
        .bindGroupLayoutCount = static_cast<uint32_t>(bindGroupLayouts.size()),
        .bindGroupLayouts = bindGroupLayouts.data()
    };
    wgpu::PipelineLayout pipelineLayout = GetRenderSystem()->GetDevice().CreatePipelineLayout(&pipelineLayoutDescriptor);

    // The sea is translucent: it depth-tests against the terrain but doesn't write depth,
    // so it never occludes anything drawn after it.
    wgpu::DepthStencilState depthState{
        .format = wgpu::TextureFormat::Depth32Float,
        .depthWriteEnabled = false,
        .depthCompare = wgpu::CompareFunction::Less
    };

    wgpu::RenderPipelineDescriptor descriptor{
        .label = "Sea render pipeline",
        .layout = pipelineLayout,
        .vertex = {
            .module = m_pShader->GetShaderModule(),
            .bufferCount = 1,
            .buffers = GetRenderSystem()->GetVertexBufferLayout(VertexFormat::VERTEX_FORMAT_P3_C4_UV) },
        .primitive = { .topology = wgpu::PrimitiveTopology::TriangleList, .cullMode = wgpu::CullMode::None },
        .depthStencil = &depthState,
        .multisample = { .count = RenderSystem::MsaaSampleCount },
        .fragment = &fragmentState
    };
    m_RenderPipeline = GetRenderSystem()->GetDevice().CreateRenderPipeline(&descriptor);
}

void SeaRenderSystem::Render(wgpu::RenderPassEncoder& renderPass)
{
    if (GetActiveScene() == nullptr)
    {
        return;
    }

    if (m_RenderPipeline && m_VertexBuffer && m_IndexBuffer && m_IndexCount > 0)
    {
        renderPass.SetPipeline(m_RenderPipeline);
        renderPass.SetVertexBuffer(0, m_VertexBuffer);
        renderPass.SetIndexBuffer(m_IndexBuffer, wgpu::IndexFormat::Uint32);
        renderPass.DrawIndexed(m_IndexCount);
    }
}

void SeaRenderSystem::HandleShaderInjection()
{
    if (!m_ShaderInjectionSignalId.has_value())
    {
        m_ShaderInjectionSignalId = GetResourceSystem()->GetShaderInjectedSignal().Connect(
            [this](ResourceShader* pResourceShader) {
                CreateRenderPipeline();
            });
    }
}

} // namespace WingsOfSteel
