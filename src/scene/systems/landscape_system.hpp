#pragma once

#include "imgui/idebugui.hpp"
#include "scene/entity.hpp"
#include "scene/systems/system.hpp"
#include "scene/components/landscape_component.hpp"

namespace WingsOfSteel
{

DECLARE_SMART_PTR(Scene);

class LandscapeSystem : public System, public IDebugUI
{
public:
    LandscapeSystem();
    ~LandscapeSystem();

    void Initialize(Scene* pScene) override{};
    void Update(float delta) override{};

    void Generate(EntitySharedPtr pLandscapeEntity, uint32_t width, uint32_t height);

    void DrawDebugUI() override;

private:
    void GenerateDebugHeightfieldTexture(LandscapeComponent& landscapeComponent);
    EntityWeakPtr m_pLandscapeEntity;
};

} // namespace WingsOfSteel
