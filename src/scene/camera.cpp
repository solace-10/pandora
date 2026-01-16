#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "pandora.hpp"
#include "render/window.hpp"
#include "scene/camera.hpp"

namespace WingsOfSteel
{

Camera::Camera(float fov, float nearPlane, float farPlane)
    : m_Fov(fov)
    , m_NearPlane(nearPlane)
    , m_FarPlane(farPlane)
{
    CalculateProjectionMatrix();
}

void Camera::LookAt(const glm::vec3& cameraPosition, const glm::vec3& targetPosition, const glm::vec3& up)
{
    m_ViewMatrix = glm::lookAt(cameraPosition, targetPosition, up);
    m_CameraPosition = cameraPosition;
    m_CameraTarget = targetPosition;
}

void Camera::SetNearPlane(float distance)
{
    m_NearPlane = distance;
    CalculateProjectionMatrix();
}

void Camera::SetFarPlane(float distance)
{
    m_FarPlane = distance;
    CalculateProjectionMatrix();
}

void Camera::SetFieldOfView(float degrees)
{
    m_Fov = glm::radians(degrees);
    CalculateProjectionMatrix();
}

const glm::mat4& Camera::GetProjectionMatrix()
{
    if (m_WindowWidth != GetWindow()->GetWidth() || m_WindowHeight != GetWindow()->GetHeight())
    {
        CalculateProjectionMatrix();
    }

    return m_ProjectionMatrix;
}

void Camera::CalculateProjectionMatrix()
{
    m_WindowWidth = GetWindow()->GetWidth();
    m_WindowHeight = GetWindow()->GetHeight();
    const float aspectRatio = static_cast<float>(m_WindowWidth) / static_cast<float>(m_WindowHeight);
    m_ProjectionMatrix = glm::perspectiveZO(m_Fov, aspectRatio, m_NearPlane, m_FarPlane);
}

glm::vec3 Camera::ScreenToWorld(const glm::vec2& mousePos, uint32_t windowWidth, uint32_t windowHeight, float depth) const
{
    // Convert screen coordinates to normalized device coordinates (-1 to 1)
    const float x = (2.0f * mousePos.x) / static_cast<float>(windowWidth) - 1.0f;
    const float y = 1.0f - (2.0f * mousePos.y) / static_cast<float>(windowHeight);

    // Create a point in clip space at the specified depth
    const glm::vec4 clipCoords(x, y, depth, 1.0f);

    // Get the inverse of the view-projection matrix
    const glm::mat4 viewProj = m_ProjectionMatrix * m_ViewMatrix;
    const glm::mat4 invViewProj = glm::inverse(viewProj);

    // Transform from clip space to world space
    glm::vec4 worldCoords = invViewProj * clipCoords;

    // Perform perspective division
    if (worldCoords.w != 0.0f)
    {
        worldCoords /= worldCoords.w;
    }

    return glm::vec3(worldCoords);
}

glm::vec3 Camera::WorldToScreen(const glm::vec3& worldPos, uint32_t windowWidth, uint32_t windowHeight) const
{
    // Transform world position to clip space
    const glm::mat4 viewProj = m_ProjectionMatrix * m_ViewMatrix;
    const glm::vec4 clipCoords = viewProj * glm::vec4(worldPos, 1.0f);

    // Perform perspective division to get normalized device coordinates
    glm::vec3 ndc;
    if (clipCoords.w != 0.0f)
    {
        ndc = glm::vec3(clipCoords) / clipCoords.w;
    }
    else
    {
        ndc = glm::vec3(clipCoords);
    }

    // Convert NDC to screen coordinates
    // x: [-1, 1] -> [0, windowWidth]
    // y: [-1, 1] -> [windowHeight, 0] (flip Y for screen space where 0,0 is top-left)
    // z: keep in [0, 1] range for depth
    const float screenX = (ndc.x + 1.0f) * 0.5f * static_cast<float>(windowWidth);
    const float screenY = (1.0f - ndc.y) * 0.5f * static_cast<float>(windowHeight);

    return glm::vec3(screenX, screenY, ndc.z);
}

} // namespace WingsOfSteel
