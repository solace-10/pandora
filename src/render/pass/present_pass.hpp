#pragma once

#include "render/pass/pass.hpp"

namespace WingsOfSteel
{

// Exposes and tonemaps the scene, which is rendered in linear space, encodes it into the surface for
// display and composites the window's overlay on top. Passes after this one draw straight onto the
// surface.
DECLARE_SMART_PTR(PresentPass);
class PresentPass : public Pass
{
public:
    PresentPass();

    void Execute(wgpu::CommandEncoder& encoder) override;

private:
    void CreateRenderPipeline();

    wgpu::RenderPipeline m_Pipeline;
    wgpu::Buffer m_ExposureBuffer;
};

} // namespace WingsOfSteel
