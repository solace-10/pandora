#include "render/rendersystem.hpp"

#include <cassert>
#include <string_view>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <magic_enum.hpp>

#include "core/log.hpp"
#include "pandora.hpp"
#include "render/lighting/lighting_system.hpp"
#include "render/mip_level_generator.hpp"
#include "render/render_pass/base_render_pass.hpp"
#include "render/render_pass/ui_render_pass.hpp"
#include "render/shader_compiler.hpp"
#include "render/shader_editor.hpp"
#include "render/vertex_buffer_schemas.hpp"
#include "render/window.hpp"
#include "scene/camera.hpp"
#include "scene/components/camera_component.hpp"
#include "scene/scene.hpp"

namespace WingsOfSteel
{

wgpu::Instance g_Instance = nullptr;
wgpu::Adapter g_Adapter = nullptr;
wgpu::Device g_Device = nullptr;
OnRenderSystemInitializedCallback g_OnRenderSystemInitializedCallback;

void OnRenderSystemInitializedWrapper()
{
    g_OnRenderSystemInitializedCallback();
}

RenderSystem::RenderSystem()
{
}

RenderSystem::~RenderSystem()
{
    glfwTerminate();
}

void RenderSystem::Initialize(OnRenderSystemInitializedCallback onInitializedCallback)
{
    g_OnRenderSystemInitializedCallback = onInitializedCallback;

    g_Instance = wgpu::CreateInstance();
    if (g_Instance)
    {
        Log::Info() << "Initialized WebGPU.";
    }
    else
    {
        Log::Error() << "Failed to initialize WebGPU.";
        exit(-1);
    }

    AcquireDevice([](wgpu::Device device) {
        g_Device = device;
        Log::Info() << "WebGPU device acquired.";

        if (!glfwInit())
        {
            Log::Error() << "Failed to initialize GLFW.";
            exit(-1);
            return;
        }

        GetRenderSystem()->InitializeInternal();
        OnRenderSystemInitializedWrapper();
    });
    // Code here will never be reached until shutdown has started.
}

void RenderSystem::InitializeInternal()
{
    CreateGlobalUniforms();
    m_pMipLevelGenerator = std::make_unique<MipLevelGenerator>();
    m_pVertexBufferSchemas = std::make_unique<VertexBufferSchemas>();
    m_pShaderCompiler = std::make_unique<ShaderCompiler>();
    m_pShaderEditor = std::make_unique<ShaderEditor>();
    m_pLightingSystem = std::make_unique<LightingSystem>();

    AddRenderPass(std::make_shared<BaseRenderPass>());
    AddRenderPass(std::make_shared<UIRenderPass>());
}

void RenderSystem::Update()
{
#if defined(TARGET_PLATFORM_NATIVE)
    // Tick needs to be called in Dawn to display validation errors
    GetDevice().Tick();
#endif

    GetShaderEditor()->Update();
    GetLightingSystem()->Update();

    wgpu::CommandEncoderDescriptor commandEncoderDescriptor{
        .label = "Pandora default command encoder"
    };
    wgpu::CommandEncoder encoder = GetDevice().CreateCommandEncoder(&commandEncoderDescriptor);

    for (auto& pRenderPass : m_RenderPasses)
    {
        pRenderPass->Render(encoder);
    }

    wgpu::CommandBufferDescriptor commandBufferDescriptor{
        .label = "Pandora default command buffer"
    };
    wgpu::CommandBuffer commands = encoder.Finish(&commandBufferDescriptor);
    GetDevice().GetQueue().Submit(1, &commands);
}

void RenderSystem::AddRenderPass(RenderPassSharedPtr pRenderPass)
{
    m_RenderPasses.push_back(pRenderPass);
}

const std::list<RenderPassSharedPtr>& RenderSystem::GetRenderPasses() const
{
    return m_RenderPasses;
}

void RenderSystem::ClearRenderPasses()
{
    m_RenderPasses.clear();
}

RenderPassSharedPtr RenderSystem::GetRenderPass(const std::string& name) const
{
    for (auto& pRenderPass : m_RenderPasses)
    {
        if (pRenderPass->GetName() == name)
        {
            return pRenderPass;
        }
    }
    return nullptr;
}

wgpu::Instance& RenderSystem::GetInstance() const
{
    return g_Instance;
}

wgpu::Adapter& RenderSystem::GetAdapter() const
{
    return g_Adapter;
}

wgpu::Device& RenderSystem::GetDevice() const
{
    return g_Device;
}

void RenderSystem::AcquireDevice(void (*callback)(wgpu::Device))
{
    // Request the high performance device if possible, so we don't accidentally select an integrated GPU when
    // a dedicated GPU is available.
    wgpu::RequestAdapterOptions adapterOptions{
        .powerPreference = wgpu::PowerPreference::HighPerformance
    };

    g_Instance.RequestAdapter(
        &adapterOptions,
        wgpu::CallbackMode::AllowSpontaneous,
        [callback](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
            if (status != wgpu::RequestAdapterStatus::Success)
            {
                Log::Error() << "Failed to request adapter: " << std::string_view(message.data, message.length);
                exit(-1);
            }

            g_Adapter = std::move(adapter);
            Log::Info() << "Adapter successfully requested.";

            wgpu::AdapterInfo info{};
            g_Adapter.GetInfo(&info);

            Log::Info() << "Adapter properties:";
            Log::Info() << " - Vendor: " << std::string_view(info.vendor.data, info.vendor.length);
            Log::Info() << " - Architecture: " << std::string_view(info.architecture.data, info.architecture.length);
            Log::Info() << " - Device: " << std::string_view(info.device.data, info.device.length);
            Log::Info() << " - Description: " << std::string_view(info.description.data, info.description.length);
            Log::Info() << " - Backend type: " << magic_enum::enum_name(info.backendType);

            wgpu::DeviceDescriptor deviceDescriptor{};
            deviceDescriptor.SetUncapturedErrorCallback(
                [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
                    Log::Error() << "Uncaptured device error: " << magic_enum::enum_name(type)
                                 << " - message: " << std::string_view(message.data, message.length);
                    exit(-1);
                });

            g_Adapter.RequestDevice(
                &deviceDescriptor,
                wgpu::CallbackMode::AllowSpontaneous,
                [callback](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
                    if (status != wgpu::RequestDeviceStatus::Success)
                    {
                        Log::Error() << "Failed to request device: " << std::string_view(message.data, message.length);
                        exit(-1);
                    }
                    callback(std::move(device));
                });
        });
}

void RenderSystem::CreateGlobalUniforms()
{
    static_assert(sizeof(GlobalUniforms) % 16 == 0);

    using namespace wgpu;

    memset(&m_GlobalUniforms, 0, sizeof(GlobalUniforms));
    m_GlobalUniforms.projectionMatrix = glm::mat4x4(1.0f);
    m_GlobalUniforms.viewMatrix = glm::mat4x4(1.0f);
    m_GlobalUniforms.cameraPosition = glm::vec4(0.0f);
    m_GlobalUniforms.time = 0.0f;
    m_GlobalUniforms.windowWidth = 0.0f;
    m_GlobalUniforms.windowHeight = 0.0f;
    m_GlobalUniforms.directionalLightColor = glm::vec4(1.0f);
    m_GlobalUniforms.directionalLightDirection = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
    m_GlobalUniforms.ambientLightColor = glm::vec4(1.0f);

    BufferDescriptor bufferDescriptor{
        .label = "Global uniforms buffer",
        .usage = BufferUsage::CopyDst | BufferUsage::Uniform,
        .size = sizeof(GlobalUniforms)
    };

    m_GlobalUniformsBuffer = GetDevice().CreateBuffer(&bufferDescriptor);

    BindGroupLayoutEntry bindGroupLayoutEntry{
        .binding = 0,
        .visibility = ShaderStage::Vertex | ShaderStage::Fragment,
        .buffer{
            .type = wgpu::BufferBindingType::Uniform,
            .minBindingSize = sizeof(GlobalUniforms) }
    };

    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor{
        .entryCount = 1,
        .entries = &bindGroupLayoutEntry
    };
    m_GlobalUniformsBindGroupLayout = GetDevice().CreateBindGroupLayout(&bindGroupLayoutDescriptor);

    BindGroupEntry bindGroupEntry{
        .binding = 0,
        .buffer = m_GlobalUniformsBuffer,
        .offset = 0,
        .size = sizeof(GlobalUniforms)
    };

    BindGroupDescriptor bindGroupDescriptor{
        .layout = m_GlobalUniformsBindGroupLayout,
        .entryCount = bindGroupLayoutDescriptor.entryCount,
        .entries = &bindGroupEntry
    };

    m_GlobalUniformsBindGroup = GetDevice().CreateBindGroup(&bindGroupDescriptor);
}

void RenderSystem::UpdateGlobalUniforms(wgpu::RenderPassEncoder& renderPass)
{
    EntitySharedPtr pCamera = GetActiveScene() ? GetActiveScene()->GetCamera() : nullptr;
    if (pCamera)
    {
        CameraComponent& cameraComponent = pCamera->GetComponent<CameraComponent>();
        m_GlobalUniforms.projectionMatrix = cameraComponent.camera.GetProjectionMatrix();
        m_GlobalUniforms.viewMatrix = cameraComponent.camera.GetViewMatrix();
        m_GlobalUniforms.cameraPosition = glm::vec4(cameraComponent.camera.GetPosition(), 1.0f);
    }
    else
    {
        m_GlobalUniforms.projectionMatrix = glm::ortho(0.0f, static_cast<float>(GetWindow()->GetWidth()), 0.0f, static_cast<float>(GetWindow()->GetHeight()));
        m_GlobalUniforms.viewMatrix = glm::mat4x4(1.0f);
        m_GlobalUniforms.cameraPosition = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    m_GlobalUniforms.time = static_cast<float>(glfwGetTime());
    m_GlobalUniforms.windowWidth = static_cast<float>(GetWindow()->GetWidth());
    m_GlobalUniforms.windowHeight = static_cast<float>(GetWindow()->GetHeight());

    const DirectionalLight& directionalLight = GetLightingSystem()->GetDirectionalLight();
    m_GlobalUniforms.directionalLightColor = glm::vec4(directionalLight.Color, 1.0f); // Needs to be a vec4 for alignment. alpha set to 1.0 to ensure any multiplications in the shader are correct.
    m_GlobalUniforms.directionalLightDirection = glm::vec4(directionalLight.Direction, 0.0f); // Needs to be a vec4 for alignment. w set to 0.0 not to affect normalization.

    const AmbientLight& ambientLight = GetLightingSystem()->GetAmbientLight();
    m_GlobalUniforms.ambientLightColor = glm::vec4(ambientLight.Color, 1.0f);

    GetDevice().GetQueue().WriteBuffer(m_GlobalUniformsBuffer, 0, &m_GlobalUniforms, sizeof(GlobalUniforms));
    renderPass.SetBindGroup(0, m_GlobalUniformsBindGroup);
}

wgpu::BindGroupLayout& RenderSystem::GetGlobalUniformsLayout()
{
    return m_GlobalUniformsBindGroupLayout;
}

const wgpu::VertexBufferLayout* RenderSystem::GetVertexBufferLayout(VertexFormat vertexFormat) const
{
    if (m_pVertexBufferSchemas)
    {
        return m_pVertexBufferSchemas->GetLayout(vertexFormat);
    }
    else
    {
        return nullptr;
    }
}

LightingSystem* RenderSystem::GetLightingSystem() const
{
    return m_pLightingSystem.get();
}

MipLevelGenerator* RenderSystem::GetMipLevelGenerator() const
{
    return m_pMipLevelGenerator.get();
}

ShaderCompiler* RenderSystem::GetShaderCompiler() const
{
    return m_pShaderCompiler.get();
}

ShaderEditor* RenderSystem::GetShaderEditor() const
{
    return m_pShaderEditor.get();
}

} // namespace WingsOfSteel
