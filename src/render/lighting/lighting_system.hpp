#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <glm/vec3.hpp>

#include "imgui/idebugui.hpp"
#include "render/rendersystem.hpp"
#include "scene/entity.hpp"

namespace WingsOfSteel
{

enum class Tonemapper : uint32_t
{
    Neutral,
    ACES,
    AgX
};

struct AmbientLight
{
    glm::vec3 Color{ 1.0f, 1.0f, 1.0f };
    float Intensity{ 1.0f };
};

// Ambient light as order 2 spherical harmonics: the 9 coefficients of the real SH basis, in the order
// l = 0, then l = 1 (y, z, x), then l = 2 (xy, yz, 3z^2 - 1, xz, x^2 - y^2). They are already convolved
// with the cosine lobe and divided by pi, so evaluated in a direction they give the light that a white
// Lambertian surface facing that way reflects.
using AmbientSH = std::array<glm::vec3, 9>;

struct DirectionalLight
{
    float Angle{ 0.0f };
    float Pitch{ 0.0f };
    glm::vec3 Direction{ 0.0f, 1.0f, 0.0f };
    glm::vec3 Color{ 1.0f, 1.0f, 1.0f };
    float Intensity{ 1.0f };
};

class LightingSystem : public IDebugUI
{
public:
    LightingSystem();
    ~LightingSystem();

    void DrawDebugUI() override;

    void Update();

    const AmbientLight& GetAmbientLight() const;
    const DirectionalLight& GetDirectionalLight() const;

    // Replaces the ambient colour with light that varies with direction, such as a sky's. The
    // ambient intensity still scales it.
    void SetAmbientSH(const AmbientSH& ambientSH);
    void ClearAmbientSH();

    // What shaders receive: the SH set above, or the ambient colour as a constant if there is none.
    AmbientSH GetAmbientSH() const;

    // In stops: the scene is scaled by 2^exposure before it is tonemapped.
    float GetExposure() const;

    // How much of the blurred scene is mixed into it; 0 turns bloom off.
    float GetBloomStrength() const;

    Tonemapper GetTonemapper() const;

private:
    void DrawDebugGizmo();

    AmbientLight m_AmbientLight;
    EntityWeakPtr m_pAmbientLightEntity;
    std::optional<AmbientSH> m_AmbientSH;

    DirectionalLight m_DirectionalLight;
    EntityWeakPtr m_pDirectionalLightEntity;

    float m_Exposure{ 1.0f };
    float m_BloomStrength{ 0.04f };
    Tonemapper m_Tonemapper{ Tonemapper::ACES };
};

} // namespace WingsOfSteel
