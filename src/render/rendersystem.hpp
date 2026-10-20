#pragma once

#include <functional>
#include <list>
#include <string>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <webgpu/webgpu_cpp.h>

#include "core/smart_ptr.hpp"
#include "render/vertex_types.hpp"

namespace WingsOfSteel
{

DECLARE_SMART_PTR(DebugRender);
DECLARE_SMART_PTR(LightingSystem);
DECLARE_SMART_PTR(MipLevelGenerator);
DECLARE_SMART_PTR(Pass);
DECLARE_SMART_PTR(ShaderCompiler);
DECLARE_SMART_PTR(ShaderEditor);
DECLARE_SMART_PTR(VertexBufferSchemas);

using OnRenderSystemInitializedCallback = std::function<void()>;

class RenderSystem
{
public:
    RenderSystem();
    ~RenderSystem();

    void Initialize(OnRenderSystemInitializedCallback onInitializedCallback);
    void Update();

    void AddPass(PassSharedPtr pPass);
    void RemovePass(const PassSharedPtr& pPass);
    const std::list<PassSharedPtr>& GetPasses() const;
    PassSharedPtr GetPass(const std::string& name) const;
    void ClearPasses();

    wgpu::Instance& GetInstance() const;
    wgpu::Adapter& GetAdapter() const;
    wgpu::Device& GetDevice() const;

    bool HasFeature(wgpu::FeatureName feature) const;

    wgpu::BindGroupLayout& GetGlobalUniformsLayout();
    const wgpu::VertexBufferLayout* GetVertexBufferLayout(VertexFormat vertexFormat) const;

    LightingSystem* GetLightingSystem() const;
    MipLevelGenerator* GetMipLevelGenerator() const;
    ShaderCompiler* GetShaderCompiler() const;
    ShaderEditor* GetShaderEditor() const;

    // The global uniforms are written once per frame, before any pass runs.
    void BindGlobalUniforms(wgpu::RenderPassEncoder& renderPassEncoder);

    // What a ShadowPass binds at group 0 instead of the global uniforms: its view projection.
    wgpu::BindGroupLayout& GetShadowViewLayout();
    wgpu::TextureView& GetShadowMapView();

    static constexpr size_t MsaaSampleCount = 4;
    static constexpr uint32_t ShadowMapSize = 2048;

private:
    void AcquireDevice(void (*callback)());
    void InitializeInternal();

    void CreateGlobalUniforms();
    void WriteGlobalUniforms();

    struct GlobalUniforms
    {
        glm::mat4x4 projectionMatrix;
        glm::mat4x4 viewMatrix;
        glm::vec4 cameraPosition;
        float time;
        float windowWidth;
        float windowHeight;
        float _padding1;
        glm::vec4 directionalLightColor;
        glm::vec4 directionalLightDirection;
        glm::vec4 ambientLightColor;
        glm::vec4 ambientSH[9]; // See AmbientSH; vec4s for alignment.
        glm::mat4x4 shadowViewProjection;
        glm::vec4 shadowParameters; // x: normal offset in world units.
    };
    GlobalUniforms m_GlobalUniforms;

    wgpu::Buffer m_GlobalUniformsBuffer;
    wgpu::BindGroup m_GlobalUniformsBindGroup;
    wgpu::BindGroupLayout m_GlobalUniformsBindGroupLayout;
    wgpu::BindGroupLayout m_ShadowViewLayout;

    wgpu::Texture m_ShadowMap;
    wgpu::TextureView m_ShadowMapView;
    wgpu::Sampler m_ShadowSampler;

    ShaderCompilerUniquePtr m_pShaderCompiler;
    ShaderEditorUniquePtr m_pShaderEditor;

    std::list<PassSharedPtr> m_Passes;
    VertexBufferSchemasUniquePtr m_pVertexBufferSchemas;

    LightingSystemUniquePtr m_pLightingSystem;
    MipLevelGeneratorUniquePtr m_pMipLevelGenerator;
};

} // namespace WingsOfSteel
