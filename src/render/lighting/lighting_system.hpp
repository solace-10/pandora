
#include <glm/vec3.hpp>

#include "imgui/idebugui.hpp"
#include "render/rendersystem.hpp"
#include "scene/entity.hpp"

namespace WingsOfSteel
{

struct DirectionalLight
{
    float Angle{ 0.0f };
    float Pitch{ 0.0f };
    glm::vec3 Direction{ 0.0f, 1.0f, 0.0f };
    glm::vec3 Color{ 1.0f, 1.0f, 1.0f };
};

class LightingSystem : public IDebugUI
{
public:
    LightingSystem();
    ~LightingSystem();

    void DrawDebugUI() override;

    void Update();

    const DirectionalLight& GetDirectionalLight() const;

private:
    void DrawDebugGizmo();

    DirectionalLight m_DirectionalLight;
    EntityWeakPtr m_pDirectionalLightEntity;
};

} // namespace WingsOfSteel
