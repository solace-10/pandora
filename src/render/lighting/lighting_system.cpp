#include <glm/glm.hpp>

#include "imgui.h"
#include "imgui/imgui.hpp"
#include "pandora.hpp"
#include "render/debug_render.hpp"
#include "render/lighting/lighting_system.hpp"
#include "render/rendersystem.hpp"
#include "scene/components/ambient_light_component.hpp"
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

        if (IsDebugUIVisible())
        {
            m_pAmbientLightEntity = GetActiveScene()->GetEntity(entityHandle);
        }
    });

    if (IsDebugUIVisible())
    {
        DrawDebugUI();
        DrawDebugGizmo();
    }
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

            float angle = m_DirectionalLight.Angle;
            if (ImGui::SliderFloat("Angle", &angle, 0.0f, 360.0f))
            {
                directionalLightComponent.SetAngle(angle);
            }

            float pitch = m_DirectionalLight.Pitch;
            if (ImGui::SliderFloat("Pitch", &pitch, 0.0f, 90.0f))
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
            if (ImGui::ColorEdit3("AmbientColor", (float*)&color, ImGuiColorEditFlags_Float))
            {
                ambientLightComponent.SetColor(color);
            }
        }
        else
        {
            ImGui::TextUnformatted("Ambient light not set.");
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
