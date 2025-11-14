#pragma once

#include <glm/vec3.hpp>
#include "icomponent.hpp"
#include "component_factory.hpp"

namespace WingsOfSteel
{

class DirectionalLightComponent : public IComponent
{
public:
    DirectionalLightComponent() = default;
    
    void Deserialize(const ResourceDataStore* pContext, const Json::Data& json) override
    {
    }

    void SetAngle(float angle);
    float GetAngle() const { return m_Angle; }
    void SetPitch(float pitch);
    float GetPitch() const { return m_Pitch; }
    const glm::vec3& GetNormalizedDirection() const { return m_Direction; }

    const glm::vec3& GetColor() const { return m_Color; }
    void SetColor(float r, float g, float b) { SetColor(glm::vec3(r, g, b)); }
    void SetColor(const glm::vec3& color) { m_Color = color; }

private:
    void CalculateNormalizedDirection();
    
    glm::vec3 m_Direction{ 0.0f, -1.0f, 0.0f };
    glm::vec3 m_Color{ 1.0f, 1.0f, 1.0f };
    float m_Angle{ 0.0f };
    float m_Pitch{ 0.0f };
};

REGISTER_COMPONENT(DirectionalLightComponent, "directional_light")

} // namespace WingsOfSteel
