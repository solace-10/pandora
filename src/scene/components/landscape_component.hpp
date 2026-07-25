#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "core/serialization.hpp"
#include "icomponent.hpp"
#include "component_factory.hpp"
#include "resources/resource.fwd.hpp"
#include "resources/resource_texture_2d.hpp"

namespace WingsOfSteel
{

class LandscapeComponent : public IComponent
{
public:
    LandscapeComponent() = default;
    ~LandscapeComponent() = default;

    uint32_t Seed{ 0 };
    uint32_t Width{ 32 };
    uint32_t Length{ 32 };
    float Height{ 40.0f }; // Maximum height the landscape will be generated to, in meters.
    float WaterLevel{ 10.0f };
    float CellSize{ 10.0f }; // Cell size, in meters. Shared by the visual mesh and the physics heightfield.
    uint32_t Generation{ 0 }; // Current generation iteration; used by the renderer to know when the component is stale.
    uint32_t Octaves{ 4 };
    float Frequency{ 0.002 };

    // Island shaping. The falloff mask is subtracted from the terrain height, rather than
    // scaling it, so relief is preserved and the coastline falls wherever the noise happens
    // to cross the water level.
    float FalloffStart{ 0.35f }; // Normalized distance from the center at which land starts being pushed down.
    float FalloffEnd{ 0.95f }; // Normalized distance at which the falloff reaches full strength.
    float FalloffStrength{ 1.0f }; // How much height is subtracted at full falloff, in normalized height.

    // Domain warping distorts the terrain before the falloff is applied, turning what would
    // otherwise be a fairly circular island into one with headlands and inlets.
    float DomainWarpAmp{ 30.0f }; // Warp distance, in cells. Set to 0 to disable warping.
    float DomainWarpFrequency{ 0.01f };

    // Continentalness biases one broad region of the map upwards, so that the terrain naturally
    // clumps into a single landmass instead of an even spread of land and water.
    float ContinentFrequency{ 0.002f };
    float ContinentStrength{ 0.5f }; // How much the continent noise is blended in. 0 disables it.

    // Redistribution flattens the lowlands while leaving the peaks alone, giving us wide beaches
    // and coastal plains. Values above 1 flatten, values below 1 raise the terrain.
    float Redistribution{ 1.6f };

    // An optional ridged fractal, blended in towards the middle of the island to give it a
    // mountainous spine. Disabled by default.
    float RidgeStrength{ 0.0f };
    float RidgeFrequency{ 0.02f };

    std::vector<float> Heightmap;
    ResourceTexture2DUniquePtr DebugHeightmapTexture;

    // Optional per-cell color, using the same layout as the heightmap. Whoever owns the landscape
    // is free to fill this in to tint the terrain; when it is empty, or doesn't match the size of
    // the heightmap, the landscape falls back to a single flat color.
    // The generation is tracked separately from the one above, as the colors can change without
    // the terrain itself being regenerated.
    std::vector<glm::vec3> VertexColors;
    uint32_t VertexColorsGeneration{ 0 };

    void Deserialize(const ResourceDataStore* pContext, const Json::Data& json) override
    {
        Seed = Json::DeserializeUnsignedInteger(pContext, json, "seed", 0);
    }
};

REGISTER_COMPONENT(LandscapeComponent, "landscape")

} // namespace WingsOfSteel
