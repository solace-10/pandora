#pragma once

#include <array>

#include <glm/vec4.hpp>

#include "component_factory.hpp"
#include "core/serialization.hpp"
#include "icomponent.hpp"

namespace WingsOfSteel
{

struct SeaWave
{
    float DirectionAngle{ 0.0f }; // Direction the wave travels in, in degrees.
    float Wavelength{ 60.0f }; // Crest-to-crest distance, in meters.
    float Steepness{ 0.3f }; // 0 is flat; combined steepness across waves should stay below 1 or crests loop over themselves.
};

class SeaComponent : public IComponent
{
public:
    SeaComponent() = default;
    ~SeaComponent() = default;

    float Width{ 2000.0f }; // Extent along the X axis, in meters, centered on the origin.
    float Length{ 2000.0f }; // Extent along the Z axis, in meters, centered on the origin.
    float CellSize{ 20.0f }; // Cell size of the sea mesh, in meters. Can be coarser than the landscape's.
    uint32_t Generation{ 1 }; // Current generation iteration; used by the renderer to know when the component is stale.

    std::array<SeaWave, 3> Waves{ {
        { 0.0f, 110.0f, 0.30f },
        { 40.0f, 60.0f, 0.25f },
        { 75.0f, 31.0f, 0.20f },
    } };
    float Amplitude{ 1.0f }; // Global multiplier on wave height.
    float Speed{ 0.85f }; // Global multiplier on wave phase speed.

    glm::vec4 ShallowColor{ 0.25f, 0.60f, 0.60f, 0.45f };
    glm::vec4 DeepColor{ 0.02f, 0.40f, 0.89f, 0.47f };
    float DepthFalloff{ 40.0f }; // Water depth, in meters, over which the shallow color fades to the deep color.

    float FoamDepth{ 5.0f }; // Water depth, in meters, of the foam band hugging the shore.
    float FoamNoiseScale{ 0.08f };
    float FoamSpeed{ 0.5f };

    float SpecularStrength{ 0.1f };
    float FresnelStrength{ 1.5f };
    float SparkleStrength{ 0.4f };
    float SparkleScale{ 0.15f };

    void Deserialize(const ResourceDataStore* pContext, const Json::Data& json) override
    {
    }
};

REGISTER_COMPONENT(SeaComponent, "sea")

} // namespace WingsOfSteel
