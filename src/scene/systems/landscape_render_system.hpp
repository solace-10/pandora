#pragma once

#include <webgpu/webgpu_cpp.h>

#include "resources/resource_model.hpp"
#include "scene/systems/system.hpp"

namespace WingsOfSteel
{

class LandscapeRenderSystem : public System
{
public:
    LandscapeRenderSystem();
    ~LandscapeRenderSystem();

    void Initialize(Scene* pScene) override{};
    void Update(float delta) override{};

    void Render(wgpu::RenderPassEncoder& renderPass);

private:
    struct InstanceData
    {
        ResourceModelWeakPtr pModel;
        ResourceModel::InstanceTransforms transforms;
    };

    std::vector<InstanceData> m_InstanceData;
};

} // namespace WingsOfSteel