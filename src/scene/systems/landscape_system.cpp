#include "scene/systems/landscape_system.hpp"

#include <FastNoise/FastNoise.h>

#include "FastNoise/Generators/Fractal.h"
#include "imgui.h"
#include "imgui/imgui.hpp"
#include "resources/resource_texture_2d.hpp"
#include "scene/components/landscape_component.hpp"
#include "scene/scene.hpp"

namespace WingsOfSteel
{

LandscapeSystem::LandscapeSystem()
{
}

LandscapeSystem::~LandscapeSystem()
{
}

void LandscapeSystem::Generate(EntitySharedPtr pLandscapeEntity)
{
    if (!pLandscapeEntity->HasComponent<LandscapeComponent>())
    {
        Log::Error() << "Entity has no LandscapeComponent.";
        return;
    }

    m_pLandscapeEntity = pLandscapeEntity;
    LandscapeComponent& landscapeComponent = pLandscapeEntity->GetComponent<LandscapeComponent>();
    GenerateInternal(landscapeComponent);
}

void LandscapeSystem::GenerateInternal(LandscapeComponent& landscapeComponent)
{
    const size_t heightmapSize = landscapeComponent.Width * landscapeComponent.Length;
    if (landscapeComponent.Heightmap.size() != heightmapSize)
    {
        landscapeComponent.Heightmap.resize(heightmapSize);
    }

    auto noiseSimplex = FastNoise::New<FastNoise::Simplex>();
    auto noiseFractal = FastNoise::New<FastNoise::FractalFBm>();

    noiseFractal->SetSource(noiseSimplex);
    noiseFractal->SetOctaveCount(landscapeComponent.Octaves);

    noiseFractal->GenUniformGrid2D(landscapeComponent.Heightmap.data(), 0, 0, landscapeComponent.Width, landscapeComponent.Length, landscapeComponent.Frequency, landscapeComponent.Seed);

    for (float& height : landscapeComponent.Heightmap)
    {
        height = (height + 1.0f) * 0.5f; // Remap from [-1, 1] to [0, 1]
    }

    landscapeComponent.Generation++;

    GenerateDebugHeightmapTexture(landscapeComponent);
}

void LandscapeSystem::GenerateDebugHeightmapTexture(LandscapeComponent& landscapeComponent)
{
    std::vector<uint8_t> textureData;
    textureData.resize(landscapeComponent.Width * landscapeComponent.Length * 4);

    for (size_t i = 0; i < landscapeComponent.Heightmap.size(); ++i)
    {
        uint8_t height = static_cast<uint8_t>(landscapeComponent.Heightmap[i] * 255.0f);
        size_t index = i * 4;
        textureData[index] = height;
        textureData[index + 1] = height;
        textureData[index + 2] = height;
        textureData[index + 3] = 255;
    }
    landscapeComponent.DebugHeightmapTexture = std::make_unique<ResourceTexture2D>("Heightmap", textureData.data(), textureData.size(), landscapeComponent.Width, landscapeComponent.Length, 4);
}

void LandscapeSystem::DrawDebugUI()
{
    if (!IsDebugUIVisible())
    {
        return;
    }

    EntitySharedPtr pLandscapeEntity = m_pLandscapeEntity.lock();
    if (!pLandscapeEntity)
    {
        return;
    }

    LandscapeComponent& landscapeComponent = pLandscapeEntity->GetComponent<LandscapeComponent>();

    ImGui::Begin("Landscape generator", &m_ShowDebugUI, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::BeginGroup();
    ImGui::PushItemWidth(300);

    int seed = static_cast<int>(landscapeComponent.Seed);
    if (ImGui::DragInt("Seed", &seed, 1.0f, 0, 65536))
    {
        landscapeComponent.Seed = static_cast<uint32_t>(seed);
    }
    
    int landscapeSize = static_cast<int>(landscapeComponent.Width);
    if (ImGui::DragInt("Size", &landscapeSize, 1.0f, 32, 1024))
    {
        landscapeComponent.Width = static_cast<uint32_t>(landscapeSize);
        landscapeComponent.Length = static_cast<uint32_t>(landscapeSize);
    }

    ImGui::DragFloat("Height", &landscapeComponent.Height, 1.0f, 8.0f, 500.0f, "%.2f");
    ImGui::DragFloat("Water level", &landscapeComponent.WaterLevel, 1.0f, 0.0f, 500.0f, "%.2f");

    int octaves = static_cast<int>(landscapeComponent.Octaves);
    if (ImGui::DragInt("Octaves", &octaves, 1.0f, 1, 10))
    {
        landscapeComponent.Octaves = static_cast<uint32_t>(octaves);
    }

    ImGui::DragFloat("Frequency", &landscapeComponent.Frequency, 0.0001f, 0.0f, 0.03f, "%.4f");
    
    if (ImGui::Button("Generate"))
    {
        GenerateInternal(landscapeComponent);
    }
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    if (landscapeComponent.DebugHeightmapTexture)
    {
        ImGui::Image(landscapeComponent.DebugHeightmapTexture->GetTextureView(), ImVec2(512, 512));
    }
    ImGui::EndGroup();
    
    ImGui::End();
}

} // namespace WingsOfSteel
