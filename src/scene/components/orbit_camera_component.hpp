#pragma once

#include <glm/vec3.hpp>

#include "icomponent.hpp"

#include "component_factory.hpp"
#include "core/smart_ptr.hpp"

namespace WingsOfSteel
{

DECLARE_SMART_PTR(Entity);

class OrbitCameraComponent : public IComponent
{
public:
    OrbitCameraComponent() {}
    ~OrbitCameraComponent() {}

    float distance{ 10.0f };
    float wantedDistance{ 10.0f };
    float distanceVelocity{ 0.0f };
    float minimumDistance{ 0.0f };
    float maximumDistance{ 10.0f };
    float orbitAngle{ 0.0f };
    float wantedOrbitAngle{ 0.0f };
    float orbitAngleVelocity{ 0.0f };
    float pitch{ 0.0f };
    float wantedPitch{ 0.0f };
    float pitchVelocity{ 0.0f };
    float minimumPitch{ 0.0f };
    float maximumPitch{ 1.0f };
    glm::vec3 anchorPosition{ 0.0f };
    glm::vec3 wantedAnchorPosition{ 0.0f };
    glm::vec3 anchorPositionVelocity{ 0.0f };

    void Deserialize(const ResourceDataStore* pContext, const Json::Data& json) override {}
};

REGISTER_COMPONENT(OrbitCameraComponent, "orbit_camera")

} // namespace WingsOfSteel
