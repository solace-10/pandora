#include "render/pass/present_pass.hpp"

#include <array>
#include <cmath>

#include "pandora.hpp"
#include "render/lighting/lighting_system.hpp"
#include "render/rendersystem.hpp"
#include "render/window.hpp"

namespace WingsOfSteel
{

// Embedded rather than loaded from /shaders, which would have every game ship its own copy.
static const char* sPresentShader = R"(
@group(0) @binding(0) var sceneTexture: texture_2d<f32>;
@group(0) @binding(1) var<uniform> exposure: f32;
@group(0) @binding(2) var overlayTexture: texture_2d<f32>;

// One triangle large enough to cover the screen.
@vertex fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f
{
    let xy = vec2f(f32((vertexIndex << 1u) & 2u), f32(vertexIndex & 2u));
    return vec4f(xy * 2.0 - 1.0, 0.0, 1.0);
}

// Khronos PBR Neutral, without its toe. The toe subtracts up to 0.04 from every channel, which suits
// a lit product but would roughly halve the brightness of a day side that sits around 0.05.
// What is left is the identity below kStartCompression and a shoulder that rolls highlights off
// towards white instead of clipping them.
const kStartCompression: f32 = 0.76;
const kDesaturation: f32 = 0.15;

fn tonemap(color: vec3f) -> vec3f
{
    let peak = max(color.r, max(color.g, color.b));
    if (peak < kStartCompression)
    {
        return color;
    }

    let d = 1.0 - kStartCompression;
    let newPeak = 1.0 - d * d / (peak + d - kStartCompression);
    let g = 1.0 - 1.0 / (kDesaturation * (peak - newPeak) + 1.0);
    return mix(color * (newPeak / peak), vec3f(newPeak), g);
}

fn linearToSrgb(linear: vec3f) -> vec3f
{
    let cutoff = linear < vec3f(0.0031308);
    let higher = vec3f(1.055) * pow(linear, vec3f(1.0 / 2.4)) - vec3f(0.055);
    let lower = linear * vec3f(12.92);
    return select(higher, lower, cutoff);
}

// The scene and the surface are the same size, so this is a texel for texel copy.
@fragment fn fragmentMain(@builtin(position) position: vec4f) -> @location(0) vec4f
{
    let texel = vec2u(position.xy);
    let color = linearToSrgb(tonemap(textureLoad(sceneTexture, texel, 0).rgb * exposure));
    let overlay = textureLoad(overlayTexture, texel, 0);
    return vec4f(color * (1.0 - overlay.a) + overlay.rgb, 1.0);
}
)";

PresentPass::PresentPass()
    : Pass("Present pass")
{
}

void PresentPass::Execute(wgpu::CommandEncoder& encoder)
{
    // Created here rather than in the constructor, as the engine's default passes are
    // constructed before the window, whose surface format the pipeline needs.
    if (!m_Pipeline)
    {
        CreateRenderPipeline();
    }

    const float exposure = std::exp2(GetRenderSystem()->GetLightingSystem()->GetExposure());
    GetRenderSystem()->GetDevice().GetQueue().WriteBuffer(m_ExposureBuffer, 0, &exposure, sizeof(exposure));

    wgpu::SurfaceTexture surfaceTexture;
    GetWindow()->GetSurface().GetCurrentTexture(&surfaceTexture);

    // Rebuilt every frame, as resizing the window replaces the scene texture.
    std::array<wgpu::BindGroupEntry, 3> bindGroupEntries{ {
        { .binding = 0, .textureView = GetWindow()->GetSceneColorTexture().GetTextureView() },
        { .binding = 1, .buffer = m_ExposureBuffer, .size = sizeof(float) },
        { .binding = 2, .textureView = GetWindow()->GetOverlayColorTexture().GetTextureView() },
    } };

    wgpu::BindGroupDescriptor bindGroupDescriptor{
        .layout = m_Pipeline.GetBindGroupLayout(0),
        .entryCount = bindGroupEntries.size(),
        .entries = bindGroupEntries.data()
    };
    wgpu::BindGroup bindGroup = GetRenderSystem()->GetDevice().CreateBindGroup(&bindGroupDescriptor);

    wgpu::RenderPassColorAttachment colorAttachment{
        .view = surfaceTexture.texture.CreateView(),
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = wgpu::Color{ 0.0, 0.0, 0.0, 1.0 }
    };

    wgpu::RenderPassDescriptor renderPassDescriptor{
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment
    };

    wgpu::RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDescriptor);
    renderPass.SetPipeline(m_Pipeline);
    renderPass.SetBindGroup(0, bindGroup);
    renderPass.Draw(3);
    renderPass.End();
}

void PresentPass::CreateRenderPipeline()
{
    wgpu::Device device = GetRenderSystem()->GetDevice();

    wgpu::ShaderSourceWGSL wgslDesc{};
    wgslDesc.code = sPresentShader;

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
        .nextInChain = &wgslDesc,
        .label = "Present shader"
    };
    wgpu::ShaderModule shaderModule = device.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState{
        .format = GetWindow()->GetSurfaceFormat()
    };

    wgpu::FragmentState fragmentState{
        .module = shaderModule,
        .targetCount = 1,
        .targets = &colorTargetState
    };

    wgpu::RenderPipelineDescriptor descriptor{
        .label = "Present pipeline",
        .vertex = {
            .module = shaderModule },
        .fragment = &fragmentState
    };

    m_Pipeline = device.CreateRenderPipeline(&descriptor);

    wgpu::BufferDescriptor bufferDescriptor{
        .label = "Present exposure buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
        .size = sizeof(float)
    };
    m_ExposureBuffer = device.CreateBuffer(&bufferDescriptor);
}

} // namespace WingsOfSteel
