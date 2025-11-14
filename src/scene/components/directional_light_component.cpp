#include "scene/components/directional_light_component.hpp"
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

namespace WingsOfSteel
{

void DirectionalLightComponent::SetAngle(float angle)
{
    m_Angle = angle;
    CalculateNormalizedDirection();
}

void DirectionalLightComponent::SetPitch(float pitch)
{
    m_Pitch = pitch;
    CalculateNormalizedDirection();
}

void DirectionalLightComponent::CalculateNormalizedDirection()
{
    // Convert angle and pitch to direction vector
    // Angle rotates around Y axis (horizontal), pitch rotates up/down
    const float cosAngle = glm::cos(m_Angle);
    const float sinAngle = glm::sin(m_Angle);
    const float cosPitch = glm::cos(m_Pitch);
    const float sinPitch = glm::sin(m_Pitch);

    // Calculate direction vector
    m_Direction.x = sinAngle * cosPitch;
    m_Direction.y = -sinPitch; // Negative because directional light points down
    m_Direction.z = cosAngle * cosPitch;

    // Normalize to ensure unit vector
    m_Direction = glm::normalize(m_Direction);
}

} // namespace WingsOfSteel
