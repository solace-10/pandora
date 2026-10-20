#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"
#include "imgui/imgui.hpp"
#include "pandora.hpp"
#include "render/debug_render.hpp"
#include "render/lighting/lighting_system.hpp"
#include "render/pass/shadow_pass.hpp"
#include "render/rendersystem.hpp"
#include "scene/components/ambient_light_component.hpp"
#include "scene/components/camera_component.hpp"
#include "scene/components/directional_light_component.hpp"
#include "scene/scene.hpp"

namespace WingsOfSteel
{

LightingSystem::LightingSystem()
{
}

LightingSystem::~LightingSystem()
{
}

void LightingSystem::Update()
{
    Scene* pActiveScene = GetActiveScene();
    if (!pActiveScene)
    {
        return;
    }

    entt::registry& registry = GetActiveScene()->GetRegistry();
    auto directionalLightView = registry.view<DirectionalLightComponent>();
    directionalLightView.each([this](const auto entityHandle, DirectionalLightComponent& directionalLightComponent) {
        m_DirectionalLight.Color = directionalLightComponent.GetColor();
        m_DirectionalLight.Intensity = directionalLightComponent.GetIntensity();
        m_DirectionalLight.Direction = directionalLightComponent.GetNormalizedDirection();
        m_DirectionalLight.Angle = directionalLightComponent.GetAngle();
        m_DirectionalLight.Pitch = directionalLightComponent.GetPitch();

        if (IsDebugUIVisible())
        {
            m_pDirectionalLightEntity = GetActiveScene()->GetEntity(entityHandle);
        }
    });

    auto ambientLightView = registry.view<AmbientLightComponent>();
    ambientLightView.each([this](const auto entityHandle, AmbientLightComponent& ambientLightComponent) {
        m_AmbientLight.Color = ambientLightComponent.GetColor();
        m_AmbientLight.Intensity = ambientLightComponent.GetIntensity();

        if (IsDebugUIVisible())
        {
            m_pAmbientLightEntity = GetActiveScene()->GetEntity(entityHandle);
        }
    });

    UpdateShadowViewProjection();

    if (IsDebugUIVisible())
    {
        DrawDebugUI();
        DrawDebugGizmo();
    }
}

void LightingSystem::UpdateShadowViewProjection()
{
    glm::vec3 center(0.0f);
    EntitySharedPtr pCamera = GetActiveScene()->GetCamera();
    if (pCamera)
    {
        center = pCamera->GetComponent<CameraComponent>().camera.GetTarget();
    }

    m_ShadowViewProjection = CalculateShadowViewProjection(center, m_DirectionalLight.Direction, m_ShadowRadius, RenderSystem::ShadowMapSize);
}

// An orthographic projection from the light, covering a sphere. A sphere keeps the covered area the
// same size however the camera turns, and snapping it to whole texels stops the shadows' edges from
// crawling as the camera moves.
// ponytail: a single map sized for Knight-One's fixed camera distance; cascades if a game needs
// shadows far from its camera's target.
glm::mat4 LightingSystem::CalculateShadowViewProjection(const glm::vec3& center, const glm::vec3& towardsLight, float radius, uint32_t mapSize)
{
    const glm::vec3 up = std::abs(towardsLight.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), -towardsLight, up);

    const float texelSize = 2.0f * radius / static_cast<float>(mapSize);
    glm::vec3 lightCenter = glm::vec3(lightView * glm::vec4(center, 1.0f));
    lightCenter.x = std::floor(lightCenter.x / texelSize) * texelSize;
    lightCenter.y = std::floor(lightCenter.y / texelSize) * texelSize;

    // The view looks down -z. Things out of view but between the sphere and the light can still cast
    // shadows into it, so the near plane is pulled a further radius towards the light.
    const float nearPlane = -lightCenter.z - 2.0f * radius;
    const float farPlane = -lightCenter.z + radius;
    const glm::mat4 lightProjection = glm::orthoRH_ZO(
        lightCenter.x - radius, lightCenter.x + radius,
        lightCenter.y - radius, lightCenter.y + radius,
        nearPlane, farPlane);

    return lightProjection * lightView;
}

void LightingSystem::DrawDebugUI()
{
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_Once);
    if (ImGui::Begin("Lighting", &m_ShowDebugUI))
    {
        ImGui::SeparatorText("Directional light");
        EntitySharedPtr pDirectionalLightEntity = m_pDirectionalLightEntity.lock();
        if (pDirectionalLightEntity)
        {
            DirectionalLightComponent& directionalLightComponent = pDirectionalLightEntity->GetComponent<DirectionalLightComponent>();

            glm::vec3 color = directionalLightComponent.GetColor();
            if (ImGui::ColorEdit3("DiffuseColor", (float*)&color, ImGuiColorEditFlags_Float))
            {
                directionalLightComponent.SetColor(color);
            }

            float intensity = directionalLightComponent.GetIntensity();
            if (ImGui::DragFloat("DiffuseIntensity", &intensity, 0.01f, 0.0f, 1000.0f))
            {
                directionalLightComponent.SetIntensity(intensity);
            }

            float angle = m_DirectionalLight.Angle;
            if (ImGui::SliderFloat("Angle", &angle, 0.0f, 360.0f))
            {
                directionalLightComponent.SetAngle(angle);
            }

            // Negative pitch is a light below the horizontal, which is where anything driven by
            // a real sun spends half the year.
            float pitch = m_DirectionalLight.Pitch;
            if (ImGui::SliderFloat("Pitch", &pitch, -90.0f, 90.0f))
            {
                directionalLightComponent.SetPitch(pitch);
            }
        }
        else
        {
            ImGui::TextUnformatted("Directional light not set.");
        }

        ImGui::SeparatorText("Ambient light");
        EntitySharedPtr pAmbientLightEntity = m_pAmbientLightEntity.lock();
        if (pAmbientLightEntity)
        {
            AmbientLightComponent& ambientLightComponent = pAmbientLightEntity->GetComponent<AmbientLightComponent>();
            glm::vec3 color = ambientLightComponent.GetColor();
            ImGui::BeginDisabled(m_AmbientSH.has_value());
            if (ImGui::ColorEdit3("AmbientColor", (float*)&color, ImGuiColorEditFlags_Float))
            {
                ambientLightComponent.SetColor(color);
            }
            ImGui::EndDisabled();

            if (m_AmbientSH.has_value())
            {
                ImGui::TextDisabled("Color overriden by the skybox.");
            }

            float intensity = ambientLightComponent.GetIntensity();
            if (ImGui::DragFloat("AmbientIntensity", &intensity, 0.01f, 0.0f, 1000.0f))
            {
                ambientLightComponent.SetIntensity(intensity);
            }
        }
        else
        {
            ImGui::TextUnformatted("Ambient light not set.");
        }

        ImGui::SeparatorText("Shadows");
        ImGui::SliderFloat("Radius", &m_ShadowRadius, 10.0f, 500.0f);
        ImGui::SliderFloat("Normal offset (texels)", &m_ShadowNormalOffsetTexels, 0.0f, 5.0f);
        ImGui::Checkbox("Show shadow map", &m_ShowShadowMap);
        if (m_ShowShadowMap)
        {
            ShadowPassSharedPtr pShadowPass = std::dynamic_pointer_cast<ShadowPass>(GetRenderSystem()->GetPass("Shadow pass"));
            if (pShadowPass)
            {
                ImGui::Image(pShadowPass->RequestDebugView(), ImVec2(512, 512));
            }
        }

        ImGui::SeparatorText("Camera");
        ImGui::SliderFloat("Exposure (EV)", &m_Exposure, -10.0f, 10.0f);
        ImGui::SliderFloat("Bloom strength", &m_BloomStrength, 0.0f, 1.0f);

        int tonemapper = static_cast<int>(m_Tonemapper);
        if (ImGui::Combo("Tonemapper", &tonemapper, "PBR Neutral\0ACES\0AgX\0"))
        {
            m_Tonemapper = static_cast<Tonemapper>(tonemapper);
        }
    }
    ImGui::End();
}

const AmbientLight& LightingSystem::GetAmbientLight() const
{
    return m_AmbientLight;
}

const DirectionalLight& LightingSystem::GetDirectionalLight() const
{
    return m_DirectionalLight;
}

void LightingSystem::SetAmbientSH(const AmbientSH& ambientSH)
{
    m_AmbientSH = ambientSH;
}

void LightingSystem::ClearAmbientSH()
{
    m_AmbientSH.reset();
}

AmbientSH LightingSystem::GetAmbientSH() const
{
    AmbientSH ambientSH{};
    if (m_AmbientSH.has_value())
    {
        ambientSH = m_AmbientSH.value();
    }
    else
    {
        // Only the constant band, scaled so that it evaluates to the ambient colour everywhere.
        ambientSH[0] = m_AmbientLight.Color / 0.282095f;
    }

    for (glm::vec3& coefficient : ambientSH)
    {
        coefficient *= m_AmbientLight.Intensity;
    }
    return ambientSH;
}

float LightingSystem::GetExposure() const
{
    return m_Exposure;
}

float LightingSystem::GetBloomStrength() const
{
    return m_BloomStrength;
}

Tonemapper LightingSystem::GetTonemapper() const
{
    return m_Tonemapper;
}

const glm::mat4& LightingSystem::GetShadowViewProjection() const
{
    return m_ShadowViewProjection;
}

float LightingSystem::GetShadowNormalOffset() const
{
    return m_ShadowNormalOffsetTexels * 2.0f * m_ShadowRadius / static_cast<float>(RenderSystem::ShadowMapSize);
}

float LightingSystem::GetShadowRadius() const
{
    return m_ShadowRadius;
}

void LightingSystem::DrawDebugGizmo()
{
    GetDebugRender()->AxisTriad(glm::mat4(1.0f), 2.5f, 50.0f);
    for (int i = 50; i > 0; i -= 10)
    {
        GetDebugRender()->Circle(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), Color::HotPink, static_cast<float>(i));
    }
    GetDebugRender()->Arrow(
        m_DirectionalLight.Direction * 50.0f, m_DirectionalLight.Direction * 10.0f, Color(m_DirectionalLight.Color.r, m_DirectionalLight.Color.g, m_DirectionalLight.Color.b), 5.0f);
}

} // namespace WingsOfSteel
