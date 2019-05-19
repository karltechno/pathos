#pragma once
#include <editor/Editor.h>

namespace gfx
{
class Scene;
}

namespace editor
{

struct GFXSceneWindow
{
	GFXSceneWindow();
	~GFXSceneWindow();

	void Draw(float _dt);

	void SetScene(gfx::Scene* _scene);

private:
	gfx::Scene* m_scene = nullptr;

	editor::ImGuiWindowHandle m_windowHandle;
};

}