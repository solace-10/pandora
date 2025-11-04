#include "scene/systems/landscape_render_system.hpp"

#include <glm/glm.hpp>

#include "pandora.hpp"
#include "render/rendersystem.hpp"
#include "render/vertex_types.hpp"
#include "render/window.hpp"
#include "resources/resource_system.hpp"
#include "scene/components/landscape_component.hpp"
#include "scene/scene.hpp"

#include <array>
#include <vector>

namespace WingsOfSteel
{

LandscapeRenderSystem::LandscapeRenderSystem()
: m_VertexCount(0)
, m_Initialized(false)
{
    // Load the landscape shader
    GetResourceSystem()->RequestResource("/shaders/landscape.wgsl", [this](ResourceSharedPtr pResource) {
        m_pShader = std::dynamic_pointer_cast<ResourceShader>(pResource);
        CreateRenderPipeline();
        m_Initialized = true;
    });
}

LandscapeRenderSystem::~LandscapeRenderSystem()
{
}

void LandscapeRenderSystem::Update(float delta)
{
    if (GetActiveScene() == nullptr)
    {
        return;
    }
    
    entt::registry& registry = GetActiveScene()->GetRegistry();
    auto view = registry.view<const LandscapeComponent>();

    view.each([this](const auto entity, const LandscapeComponent& landscapeComponent) {
        if (landscapeComponent.Generation == this->m_Generation)
        {
            return;
        }

        GenerateGeometry(landscapeComponent);
    });

}

void LandscapeRenderSystem::GenerateGeometry(const LandscapeComponent& landscapeComponent)
{
    const uint32_t landscapeSize = landscapeComponent.Width;
    constexpr float cellSize = 1.0f;
    const float halfGridSize = (landscapeSize * cellSize) / 2.0f;

    const float maxHeight = 80.0f;
    const glm::vec3 color = glm::vec3(0.3f, 0.5f, 0.3f); // Single green color for all vertices

    auto getHeight = [&landscapeComponent, maxHeight](int x, int z) -> float {
        x = glm::clamp(x, 0, (int)(landscapeComponent.Width - 1));
        z = glm::clamp(z, 0, (int)(landscapeComponent.Height - 1));

        return landscapeComponent.Heightmap[x + z * landscapeComponent.Height] * maxHeight - maxHeight / 2.0f;
    };

    // Generate unique vertices: (landscapeSize+1) x (landscapeSize+1)
    std::vector<VertexP3C3N3> vertices;
    const uint32_t vertexGridSize = landscapeSize + 1;
    vertices.reserve(vertexGridSize * vertexGridSize);

    for (int z = 0; z < vertexGridSize; ++z)
    {
        for (int x = 0; x < vertexGridSize; ++x)
        {
            float worldX = x * cellSize - halfGridSize;
            float worldZ = z * cellSize - halfGridSize;
            float height = getHeight(x, z);

            // Calculate normal using neighboring heights
            // Sample heights around this vertex to compute tangent vectors
            float heightLeft = getHeight(x - 1, z);
            float heightRight = getHeight(x + 1, z);
            float heightDown = getHeight(x, z - 1);
            float heightUp = getHeight(x, z + 1);

            // Compute tangent vectors
            glm::vec3 tangentX = glm::vec3(2.0f * cellSize, heightRight - heightLeft, 0.0f);
            glm::vec3 tangentZ = glm::vec3(0.0f, heightUp - heightDown, 2.0f * cellSize);

            // Normal is the cross product of tangents
            glm::vec3 normal = glm::normalize(glm::cross(tangentZ, tangentX));

            vertices.push_back({ glm::vec3(worldX, height, worldZ), color, normal });
        }
    }

    // Generate indices for triangles
    std::vector<uint32_t> indices;
    indices.reserve(landscapeSize * landscapeSize * 6); // 2 triangles per cell, 3 indices per triangle

    for (uint32_t z = 0; z < landscapeSize; ++z)
    {
        for (uint32_t x = 0; x < landscapeSize; ++x)
        {
            // Calculate vertex indices for this quad
            uint32_t topLeft = z * vertexGridSize + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (z + 1) * vertexGridSize + x;
            uint32_t bottomRight = bottomLeft + 1;

            // First triangle (counter-clockwise from above: bottomRight, topRight, topLeft)
            indices.push_back(bottomRight);
            indices.push_back(topRight);
            indices.push_back(topLeft);

            // Second triangle (counter-clockwise from above: bottomLeft, bottomRight, topLeft)
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
            indices.push_back(topLeft);
        }
    }

    m_VertexCount = static_cast<uint32_t>(vertices.size());
    m_IndexCount = static_cast<uint32_t>(indices.size());

    wgpu::Device device = GetRenderSystem()->GetDevice();

    // Create vertex buffer
    if (m_VertexCount > 0)
    {
        wgpu::BufferDescriptor bufferDescriptor{
            .label = "Landscape vertex buffer",
            .usage = wgpu::BufferUsage::Vertex,
            .size = vertices.size() * sizeof(VertexP3C3N3),
            .mappedAtCreation = true
        };
        m_VertexBuffer = device.CreateBuffer(&bufferDescriptor);
        memcpy(m_VertexBuffer.GetMappedRange(), vertices.data(), vertices.size() * sizeof(VertexP3C3N3));
        m_VertexBuffer.Unmap();
    }

    // Create index buffer
    if (m_IndexCount > 0)
    {
        wgpu::BufferDescriptor bufferDescriptor{
            .label = "Landscape index buffer",
            .usage = wgpu::BufferUsage::Index,
            .size = indices.size() * sizeof(uint32_t),
            .mappedAtCreation = true
        };
        m_IndexBuffer = device.CreateBuffer(&bufferDescriptor);
        memcpy(m_IndexBuffer.GetMappedRange(), indices.data(), indices.size() * sizeof(uint32_t));
        m_IndexBuffer.Unmap();
    }

    m_Generation = landscapeComponent.Generation;
}

void LandscapeRenderSystem::CreateRenderPipeline()
{
    if (!m_pShader)
    {
        return;
    }

    wgpu::ColorTargetState colorTargetState{
        .format = GetWindow()->GetTextureFormat()
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

    // Depth state
    wgpu::DepthStencilState depthState{
        .format = wgpu::TextureFormat::Depth32Float,
        .depthWriteEnabled = true,
        .depthCompare = wgpu::CompareFunction::Less
    };

    // Create the render pipeline
    wgpu::RenderPipelineDescriptor descriptor{
        .label = "Landscape render pipeline",
        .layout = pipelineLayout,
        .vertex = {
            .module = m_pShader->GetShaderModule(),
            .bufferCount = 1,
            .buffers = GetRenderSystem()->GetVertexBufferLayout(VertexFormat::VERTEX_FORMAT_P3_C3_N3)
        },
        .primitive = {
            .topology = wgpu::PrimitiveTopology::TriangleList,
            .cullMode = wgpu::CullMode::Back
        },
        .depthStencil = &depthState,
        .multisample = {
            .count = RenderSystem::MsaaSampleCount
        },
        .fragment = &fragmentState
    };
    m_RenderPipeline = GetRenderSystem()->GetDevice().CreateRenderPipeline(&descriptor);
}

void LandscapeRenderSystem::Render(wgpu::RenderPassEncoder& renderPass)
{
    if (GetActiveScene() == nullptr)
    {
        return;
    }

    // Draw the grid if the pipeline is ready and we have vertices and indices
    if (m_RenderPipeline && m_VertexBuffer && m_IndexBuffer && m_IndexCount > 0)
    {
        renderPass.SetPipeline(m_RenderPipeline);
        renderPass.SetVertexBuffer(0, m_VertexBuffer);
        renderPass.SetIndexBuffer(m_IndexBuffer, wgpu::IndexFormat::Uint32);
        renderPass.DrawIndexed(m_IndexCount);
    }
}

} // namespace WingsOfSteel
