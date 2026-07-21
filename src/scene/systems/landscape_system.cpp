#include "scene/systems/landscape_system.hpp"

#include <vector>

#include <FastNoiseLite.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"
#include "imgui/imgui.hpp"
#include "physics/collision_shape.hpp"
#include "resources/resource_texture_2d.hpp"
#include "scene/components/landscape_component.hpp"
#include "scene/components/rigid_body_component.hpp"
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

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(landscapeComponent.Octaves);
    noise.SetFrequency(landscapeComponent.Frequency);
    noise.SetSeed(landscapeComponent.Seed);

    for (int y = 0; y < landscapeComponent.Length; y++)
    {
        for (int x = 0; x < landscapeComponent.Width; x++)
        {
            landscapeComponent.Heightmap[y * landscapeComponent.Width + x] = noise.GetNoise(static_cast<float>(x), static_cast<float>(y));
        }
    }

    for (float& height : landscapeComponent.Heightmap)
    {
        height = (height + 1.0f) * 0.5f; // Remap from [-1, 1] to [0, 1]
    }

    landscapeComponent.Generation++;

    GenerateDebugHeightmapTexture(landscapeComponent);

    if (EntitySharedPtr pLandscapeEntity = m_pLandscapeEntity.lock())
    {
        BuildPhysics(pLandscapeEntity, landscapeComponent);
    }
}

void LandscapeSystem::BuildPhysics(EntitySharedPtr pLandscapeEntity, const LandscapeComponent& landscapeComponent)
{
    // Build a physics heightfield that matches the visual mesh exactly. This mirrors the
    // vertex generation in LandscapeRenderSystem::GenerateGeometry: a (Width+1) x (Length+1)
    // grid of points, sampling the same clamped heights and using the same shared CellSize.
    const uint32_t gridWidth = landscapeComponent.Width + 1;
    const uint32_t gridLength = landscapeComponent.Length + 1;
    const float maxHeight = landscapeComponent.Height;
    const float waterLevel = landscapeComponent.WaterLevel;

    auto getHeight = [&landscapeComponent, maxHeight, waterLevel](int x, int z) -> float {
        x = glm::clamp(x, 0, static_cast<int>(landscapeComponent.Width) - 1);
        z = glm::clamp(z, 0, static_cast<int>(landscapeComponent.Length) - 1);
        return landscapeComponent.Heightmap[x + z * landscapeComponent.Length] * maxHeight - waterLevel;
    };

    // Row-major, x fastest, to match btHeightfieldTerrainShape's expected data layout.
    std::vector<float> heights;
    heights.reserve(static_cast<size_t>(gridWidth) * gridLength);
    for (uint32_t z = 0; z < gridLength; ++z)
    {
        for (uint32_t x = 0; x < gridWidth; ++x)
        {
            heights.push_back(getHeight(static_cast<int>(x), static_cast<int>(z)));
        }
    }

    auto pShape = std::make_shared<CollisionShapeHeightfield>(gridWidth, gridLength, std::move(heights), landscapeComponent.CellSize);

    // Bullet centres the heightfield vertically; lift the body so the raw heights line up with
    // the rendered mesh (which places vertices at their world-space height directly).
    const glm::mat4x4 worldTransform = glm::translate(glm::mat4x4(1.0f), glm::vec3(0.0f, pShape->GetVerticalOffset(), 0.0f));

    // Rebuild from scratch on regeneration so the previous body is removed from the world.
    if (pLandscapeEntity->HasComponent<RigidBodyComponent>())
    {
        pLandscapeEntity->RemoveComponent<RigidBodyComponent>();
    }

    RigidBodyComponent& rigidBodyComponent = pLandscapeEntity->AddComponent<RigidBodyComponent>();
    rigidBodyComponent.CreateStatic(pShape, worldTransform);
    rigidBodyComponent.SetOwner(pLandscapeEntity);
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
    landscapeComponent.DebugHeightmapTexture = std::make_unique<ResourceTexture2D>("Heightmap", ColorSpace::Linear, textureData.data(), textureData.size(), landscapeComponent.Width, landscapeComponent.Length, 4);
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
