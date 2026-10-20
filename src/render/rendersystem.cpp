#include "render/rendersystem.hpp"

#include <array>
#include <cassert>
#include <string_view>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <magic_enum.hpp>

#include "core/log.hpp"
#include "pandora.hpp"
#include "render/lighting/lighting_system.hpp"
#include "render/mip_level_generator.hpp"
#include "render/pass/base_render_pass.hpp"
#include "render/pass/present_pass.hpp"
#include "render/pass/shadow_pass.hpp"
#include "render/pass/ui_render_pass.hpp"
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
    ClearPasses();
    m_pLightingSystem.reset();
    m_pShaderEditor.reset();
    m_pShaderCompiler.reset();
    m_pVertexBufferSchemas.reset();
    m_pMipLevelGenerator.reset();
    m_GlobalUniformsBindGroup = nullptr;
    m_GlobalUniformsBindGroupLayout = nullptr;
    m_ShadowViewLayout = nullptr;
    m_GlobalUniformsBuffer = nullptr;
    m_ShadowSampler = nullptr;
    m_ShadowMapView = nullptr;
    m_ShadowMap = nullptr;

    g_Device = nullptr;
    g_Adapter = nullptr;
    g_Instance = nullptr;

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

    AcquireDevice([]() {
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

    AddPass(std::make_shared<ShadowPass>());
    AddPass(std::make_shared<BaseRenderPass>());
    AddPass(std::make_shared<PresentPass>());
    AddPass(std::make_shared<UIRenderPass>());
}

void RenderSystem::Update()
{
#if defined(TARGET_PLATFORM_NATIVE)
    // Tick needs to be called in Dawn to display validation errors
    GetDevice().Tick();
#endif

    GetShaderEditor()->Update();
    GetLightingSystem()->Update();
    WriteGlobalUniforms();

    wgpu::CommandEncoderDescriptor commandEncoderDescriptor{
        .label = "Pandora default command encoder"
    };
    wgpu::CommandEncoder encoder = GetDevice().CreateCommandEncoder(&commandEncoderDescriptor);

    for (auto& pPass : m_Passes)
    {
        pPass->Execute(encoder);
    }

    wgpu::CommandBufferDescriptor commandBufferDescriptor{
        .label = "Pandora default command buffer"
    };
    wgpu::CommandBuffer commands = encoder.Finish(&commandBufferDescriptor);
    GetDevice().GetQueue().Submit(1, &commands);
}

void RenderSystem::AddPass(PassSharedPtr pPass)
{
    m_Passes.push_back(pPass);
}

// Passes owned by a scene system must be withdrawn when that system goes away, as the
// pass list outlives any individual scene.
void RenderSystem::RemovePass(const PassSharedPtr& pPass)
{
    m_Passes.remove(pPass);
}

const std::list<PassSharedPtr>& RenderSystem::GetPasses() const
{
    return m_Passes;
}

void RenderSystem::ClearPasses()
{
    m_Passes.clear();
}

PassSharedPtr RenderSystem::GetPass(const std::string& name) const
{
    for (auto& pPass : m_Passes)
    {
        if (pPass->GetName() == name)
        {
            return pPass;
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

bool RenderSystem::HasFeature(wgpu::FeatureName feature) const
{
    return g_Device && g_Device.HasFeature(feature);
}

void RenderSystem::AcquireDevice(void (*callback)())
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

            // Compressed texture formats are opt-in, and requesting one the adapter does not
            // have makes RequestDevice fail outright, so each is gated on the adapter having
            // it. Which of these is available varies: desktop GPUs give us BC, mobile tends
            // towards ETC2 and ASTC. Whatever is enabled here decides what KTX2 textures are
            // transcoded to at load time.
            std::vector<wgpu::FeatureName> requiredFeatures;
            for (wgpu::FeatureName feature : { wgpu::FeatureName::TextureCompressionBC,
                     wgpu::FeatureName::TextureCompressionETC2,
                     wgpu::FeatureName::TextureCompressionASTC })
            {
                if (g_Adapter.HasFeature(feature))
                {
                    requiredFeatures.push_back(feature);
                }
            }

            if (requiredFeatures.empty())
            {
                Log::Warning() << " - No compressed texture formats are supported, textures will use more memory than intended.";
            }
            else
            {
                for (wgpu::FeatureName feature : requiredFeatures)
                {
                    Log::Info() << " - Enabling feature: " << magic_enum::enum_name(feature);
                }
            }

            wgpu::DeviceDescriptor deviceDescriptor{};
            deviceDescriptor.requiredFeatureCount = requiredFeatures.size();
            deviceDescriptor.requiredFeatures = requiredFeatures.data();
            deviceDescriptor.SetUncapturedErrorCallback(
                [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
                    Log::Error() << "Uncaptured device error: " << magic_enum::enum_name(type)
                                 << " - message: " << std::string_view(message.data, message.length);
                    exit(-1);
                });
            // Can fire from inside ~RenderSystem, so it mustn't touch engine state.
            deviceDescriptor.SetDeviceLostCallback(
                wgpu::CallbackMode::AllowSpontaneous,
                [](const wgpu::Device&, wgpu::DeviceLostReason reason, wgpu::StringView message) {
                    // Destroyed and CallbackCancelled are the normal shutdown path.
                    if (reason == wgpu::DeviceLostReason::Destroyed || reason == wgpu::DeviceLostReason::CallbackCancelled)
                    {
                        return;
                    }

                    // Recovering would mean recreating every GPU resource, so treat a real loss as fatal (Log::Error aborts).
                    Log::Error() << "Device lost: " << magic_enum::enum_name(reason)
                                 << " - message: " << std::string_view(message.data, message.length);
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
                    g_Device = std::move(device);
                    callback();
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
    m_GlobalUniforms.shadowViewProjection = glm::mat4x4(1.0f);

    BufferDescriptor bufferDescriptor{
        .label = "Global uniforms buffer",
        .usage = BufferUsage::CopyDst | BufferUsage::Uniform,
        .size = sizeof(GlobalUniforms)
    };

    m_GlobalUniformsBuffer = GetDevice().CreateBuffer(&bufferDescriptor);

    TextureDescriptor shadowMapDescriptor{
        .label = "Shadow map",
        .usage = TextureUsage::RenderAttachment | TextureUsage::TextureBinding,
        .size{ .width = ShadowMapSize, .height = ShadowMapSize },
        .format = TextureFormat::Depth32Float
    };
    m_ShadowMap = GetDevice().CreateTexture(&shadowMapDescriptor);
    m_ShadowMapView = m_ShadowMap.CreateView();

    // Linear filtering on a comparison sampler blends the results of four comparisons: 2x2 PCF.
    SamplerDescriptor shadowSamplerDescriptor{
        .label = "Shadow sampler",
        .magFilter = FilterMode::Linear,
        .minFilter = FilterMode::Linear,
        .compare = CompareFunction::LessEqual
    };
    m_ShadowSampler = GetDevice().CreateSampler(&shadowSamplerDescriptor);

    std::array<BindGroupLayoutEntry, 3> bindGroupLayoutEntries{
        BindGroupLayoutEntry{
            .binding = 0,
            .visibility = ShaderStage::Vertex | ShaderStage::Fragment,
            .buffer{
                .type = wgpu::BufferBindingType::Uniform,
                .minBindingSize = sizeof(GlobalUniforms) } },
        BindGroupLayoutEntry{
            .binding = 1,
            .visibility = ShaderStage::Fragment,
            .texture{ .sampleType = TextureSampleType::Depth } },
        BindGroupLayoutEntry{
            .binding = 2,
            .visibility = ShaderStage::Fragment,
            .sampler{ .type = SamplerBindingType::Comparison } }
    };

    BindGroupLayoutDescriptor bindGroupLayoutDescriptor{
        .label = "Global uniforms",
        .entryCount = bindGroupLayoutEntries.size(),
        .entries = bindGroupLayoutEntries.data()
    };
    m_GlobalUniformsBindGroupLayout = GetDevice().CreateBindGroupLayout(&bindGroupLayoutDescriptor);

    std::array<BindGroupEntry, 3> bindGroupEntries{
        BindGroupEntry{
            .binding = 0,
            .buffer = m_GlobalUniformsBuffer,
            .offset = 0,
            .size = sizeof(GlobalUniforms) },
        BindGroupEntry{
            .binding = 1,
            .textureView = m_ShadowMapView },
        BindGroupEntry{
            .binding = 2,
            .sampler = m_ShadowSampler }
    };

    BindGroupDescriptor bindGroupDescriptor{
        .label = "Global uniforms",
        .layout = m_GlobalUniformsBindGroupLayout,
        .entryCount = bindGroupEntries.size(),
        .entries = bindGroupEntries.data()
    };
    m_GlobalUniformsBindGroup = GetDevice().CreateBindGroup(&bindGroupDescriptor);

    // A ShadowPass renders into a shadow map, so it can't bind the global uniforms, which sample it.
    // It binds its own view projection instead.
    BindGroupLayoutEntry shadowViewLayoutEntry{
        .binding = 0,
        .visibility = ShaderStage::Vertex,
        .buffer{
            .type = wgpu::BufferBindingType::Uniform,
            .minBindingSize = sizeof(glm::mat4x4) }
    };
    BindGroupLayoutDescriptor shadowViewLayoutDescriptor{
        .label = "Shadow view",
        .entryCount = 1,
        .entries = &shadowViewLayoutEntry
    };
    m_ShadowViewLayout = GetDevice().CreateBindGroupLayout(&shadowViewLayoutDescriptor);
}

void RenderSystem::WriteGlobalUniforms()
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
    m_GlobalUniforms.directionalLightColor = glm::vec4(directionalLight.Color * directionalLight.Intensity, 1.0f); // Needs to be a vec4 for alignment. alpha set to 1.0 to ensure any multiplications in the shader are correct.
    m_GlobalUniforms.directionalLightDirection = glm::vec4(directionalLight.Direction, 0.0f); // Needs to be a vec4 for alignment. w set to 0.0 not to affect normalization.

    const AmbientLight& ambientLight = GetLightingSystem()->GetAmbientLight();
    m_GlobalUniforms.ambientLightColor = glm::vec4(ambientLight.Color * ambientLight.Intensity, 1.0f);

    const AmbientSH ambientSH = GetLightingSystem()->GetAmbientSH();
    for (size_t i = 0; i < ambientSH.size(); ++i)
    {
        m_GlobalUniforms.ambientSH[i] = glm::vec4(ambientSH[i], 0.0f);
    }

    m_GlobalUniforms.shadowViewProjection = GetLightingSystem()->GetShadowViewProjection();
    m_GlobalUniforms.shadowParameters = glm::vec4(GetLightingSystem()->GetShadowNormalOffset(), 0.0f, 0.0f, 0.0f);

    GetDevice().GetQueue().WriteBuffer(m_GlobalUniformsBuffer, 0, &m_GlobalUniforms, sizeof(GlobalUniforms));
}

void RenderSystem::BindGlobalUniforms(wgpu::RenderPassEncoder& renderPass)
{
    renderPass.SetBindGroup(0, m_GlobalUniformsBindGroup);
}

wgpu::BindGroupLayout& RenderSystem::GetShadowViewLayout()
{
    return m_ShadowViewLayout;
}

wgpu::TextureView& RenderSystem::GetShadowMapView()
{
    return m_ShadowMapView;
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
