#pragma once

#include <functional>

#include "render/pass/pass.hpp"

namespace WingsOfSteel
{

using PreSceneCallback = std::function<void(wgpu::RenderPassEncoder&)>;
using PostSceneCallback = std::function<void(wgpu::RenderPassEncoder&)>;

DECLARE_SMART_PTR(BaseRenderPass);
class BaseRenderPass : public Pass
{
public:
    BaseRenderPass();

    void Execute(wgpu::CommandEncoder& encoder) override;

    // Called before any of the scene is drawn, with the global uniforms already bound. Lets a game
    // draw backgrounds such as a skybox into the same pass, behind everything else.
    void SetPreSceneCallback(PreSceneCallback callback);

    // Called after all of the scene is drawn, translucent geometry included. Lets a game draw
    // effects such as tracers into the same HDR pass, so they are tonemapped and bloomed.
    void SetPostSceneCallback(PostSceneCallback callback);

private:
    PreSceneCallback m_PreSceneCallback;
    PostSceneCallback m_PostSceneCallback;
};

} // namespace WingsOfSteel
