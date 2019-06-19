#pragma once
#include <editor/Editor.h>

namespace gfx
{
class Scene;
struct Camera;
}

namespace editor
{

struct GFXSceneWindow
{
	GFXSceneWindow();
	~GFXSceneWindow();

	void Draw(float _dt);

	void SetScene(gfx::Scene* _scene);
	void SetMainViewCamera(gfx::Camera* _cam);

	gfx::Scene* m_scene = nullptr;
	gfx::Camera* m_cam = nullptr;

	uint32_t m_selectedLightIdx = 0xFFFFFFFF;
	bool m_guizmoEnabled = false;

	editor::ImGuiWindowHandle m_windowHandle;
};

}