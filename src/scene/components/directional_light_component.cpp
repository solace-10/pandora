#include "scene/components/directional_light_component.hpp"
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

namespace WingsOfSteel
{

void DirectionalLightComponent::SetAngle(float angleInDegrees)
{
    m_Angle = angleInDegrees;
    CalculateNormalizedDirection();
}

void DirectionalLightComponent::SetPitch(float pitchInDegrees)
{
    m_Pitch = pitchInDegrees;
    CalculateNormalizedDirection();
}

void DirectionalLightComponent::CalculateNormalizedDirection()
{
    // Convert angle and pitch to direction vector
    // Angle rotates around Y axis (horizontal), pitch rotates up/down
    const float angleInRadians = glm::radians(m_Angle);
    const float pitchInRadians = glm::radians(m_Pitch);
    const float cosAngle = glm::cos(angleInRadians);
    const float sinAngle = glm::sin(angleInRadians);
    const float cosPitch = glm::cos(pitchInRadians);
    const float sinPitch = glm::sin(pitchInRadians);

    // Calculate direction vector pointing TO the light (opposite of ray direction)
    // This is used in shaders as: NdotL = dot(normal, lightDirection)
    m_Direction.x = -sinAngle * cosPitch;
    m_Direction.y = sinPitch; // Positive for light coming from above
    m_Direction.z = -cosAngle * cosPitch;

    // Normalize to ensure unit vector
    m_Direction = glm::normalize(m_Direction);
}

} // namespace WingsOfSteel
