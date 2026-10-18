#pragma once

#include <array>
#include <functional>

#include <imgui.h>
#include <magic_enum.hpp>
#include <webgpu/webgpu_cpp.h>

#include "core/smart_ptr.hpp"

namespace WingsOfSteel
{

DECLARE_SMART_PTR(ProfilingOverlay);

using ImGuiGameMenuBarCallback = std::function<void()>;

enum class Font
{
    EXO2_REGULAR_22,
    EXO2_SEMIBOLD_22,
    EXO2_SEMIBOLD_32,
    UBUNTU_MONO_18,
    SUPPLY_MONO_SEMIBOLD_18,
    SUPPLY_MONO_REGULAR_22,
    SUPPLY_MONO_REGULAR_32
};

// The ImGuiSystem handles various implementation details for ImGui, handling
// the integration between the engine and the GLFW/WebGPU backends provided by
// ImGui.
class ImGuiSystem
{
public:
    ImGuiSystem();
    ~ImGuiSystem();

    void OnFrameStart();
    void Update();
    void Render(wgpu::RenderPassEncoder& pass);

    void SetGameMenuBarCallback(ImGuiGameMenuBarCallback callback);

    // While hidden, only the foreground draw list is rendered (e.g. the locked cursor) and ImGui
    // ignores mouse and keyboard input.
    void Show(bool state);
    bool IsVisible() const;

    ImFont* GetFont(Font font) const;

private:
    void RegisterFonts();
    void RegisterFont(Font font, ImFont* pImFont);
    void ApplyStyle();

    bool m_Visible{ true };
    bool m_ShowDemoWindow{ false };
    bool m_DebugDrawDemo{ false };
    bool m_ShowShaderEditor{ false };
    ImGuiGameMenuBarCallback m_GameMenuBarCallback;
    std::array<ImFont*, magic_enum::enum_count<Font>()> m_Fonts;
    ProfilingOverlayUniquePtr m_pProfilingOverlay;
};

inline bool ImGuiSystem::IsVisible() const
{
    return m_Visible;
}

inline ImFont* ImGuiSystem::GetFont(Font font) const
{
    return m_Fonts[static_cast<size_t>(font)];
}

} // namespace WingsOfSteel
