#include "scene/systems/landscape_render_system.hpp"

#include "pandora.hpp"
#include "render/rendersystem.hpp"
#include "render/vertex_types.hpp"
#include "render/window.hpp"
#include "resources/resource_system.hpp"
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
        GenerateGridGeometry();
        CreateRenderPipeline();
        m_Initialized = true;
    });
}

LandscapeRenderSystem::~LandscapeRenderSystem()
{
}

void LandscapeRenderSystem::GenerateGridGeometry()
{
    constexpr int gridSize = 32;
    constexpr float cellSize = 10.0f;
    constexpr float halfGridSize = (gridSize * cellSize) / 2.0f;

    std::vector<VertexP3C3> vertices;
    vertices.reserve(gridSize * gridSize * 6); // 2 triangles per cell, 3 vertices per triangle

    // Generate grid on XZ plane
    for (int z = 0; z < gridSize; ++z)
    {
        for (int x = 0; x < gridSize; ++x)
        {
            // Calculate world positions (centered at origin)
            float x0 = x * cellSize - halfGridSize;
            float x1 = (x + 1) * cellSize - halfGridSize;
            float z0 = z * cellSize - halfGridSize;
            float z1 = (z + 1) * cellSize - halfGridSize;

            // Create a checkerboard color pattern
            bool isEven = ((x + z) % 2) == 0;
            glm::vec3 color = isEven ? glm::vec3(0.3f, 0.5f, 0.3f) : glm::vec3(0.2f, 0.4f, 0.2f);

            // Define quad corners
            glm::vec3 v0(x0, 0.0f, z0); // Bottom-left
            glm::vec3 v1(x1, 0.0f, z0); // Bottom-right
            glm::vec3 v2(x1, 0.0f, z1); // Top-right
            glm::vec3 v3(x0, 0.0f, z1); // Top-left

            // First triangle (v2, v1, v0) - counter-clockwise from above
            vertices.push_back({ v2, color });
            vertices.push_back({ v1, color });
            vertices.push_back({ v0, color });

            // Second triangle (v3, v2, v0) - counter-clockwise from above
            vertices.push_back({ v3, color });
            vertices.push_back({ v2, color });
            vertices.push_back({ v0, color });
        }
    }

    m_VertexCount = static_cast<uint32_t>(vertices.size());

    // Create vertex buffer
    if (m_VertexCount > 0)
    {
        wgpu::Device device = GetRenderSystem()->GetDevice();
        wgpu::BufferDescriptor bufferDescriptor{
            .label = "Landscape vertex buffer",
            .usage = wgpu::BufferUsage::Vertex,
            .size = vertices.size() * sizeof(VertexP3C3),
            .mappedAtCreation = true
        };
        m_VertexBuffer = device.CreateBuffer(&bufferDescriptor);
        memcpy(m_VertexBuffer.GetMappedRange(), vertices.data(), vertices.size() * sizeof(VertexP3C3));
        m_VertexBuffer.Unmap();
    }
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
            .buffers = GetRenderSystem()->GetVertexBufferLayout(VertexFormat::VERTEX_FORMAT_P3_C3)
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

    // Draw the grid if the pipeline is ready and we have vertices
    if (m_RenderPipeline && m_VertexBuffer && m_VertexCount > 0)
    {
        renderPass.SetPipeline(m_RenderPipeline);
        renderPass.SetVertexBuffer(0, m_VertexBuffer);
        renderPass.Draw(m_VertexCount);
    }
}

} // namespace WingsOfSteel
