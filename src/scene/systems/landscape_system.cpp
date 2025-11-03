#include "scene/systems/landscape_system.hpp"

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

void LandscapeSystem::Generate(EntitySharedPtr pLandscapeEntity, uint32_t width, uint32_t height)
{
    if (!pLandscapeEntity->HasComponent<LandscapeComponent>())
    {
        Log::Error() << "Entity has no LandscapeComponent.";
        return;
    }

    m_pLandscapeEntity = pLandscapeEntity;

    LandscapeComponent& landscapeComponent = pLandscapeEntity->GetComponent<LandscapeComponent>();
    landscapeComponent.Width = width;
    landscapeComponent.Height = height;
    landscapeComponent.Heightmap.resize(width * height);

    landscapeComponent.Generation++;

    GenerateDebugHeightfieldTexture(landscapeComponent);
}

void LandscapeSystem::GenerateDebugHeightfieldTexture(LandscapeComponent& landscapeComponent)
{
    uint8_t height = 128;
    std::vector<uint8_t> textureData;
    textureData.resize(landscapeComponent.Width * landscapeComponent.Height * 4);
    for (size_t index = 0; index < textureData.size(); index += 4)
    {
        textureData[index    ] = height;
        textureData[index + 1] = height;
        textureData[index + 2] = height;
        textureData[index + 3] = 255;
    }
    landscapeComponent.DebugHeightmapTexture = std::make_unique<ResourceTexture2D>("Heightmap", textureData.data(), textureData.size(), landscapeComponent.Width, landscapeComponent.Height, 4);
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

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Landscape generator", &m_ShowDebugUI);

    ImGui::BeginGroup();
    ImGui::Image(landscapeComponent.DebugHeightmapTexture->GetTextureView(), ImVec2(landscapeComponent.Width, landscapeComponent.Height));
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    if (ImGui::Button("Generate"))
    {
        // Placeholder - will implement generation logic later
    }
    ImGui::EndGroup();
    ImGui::End();
}

} // namespace WingsOfSteel
