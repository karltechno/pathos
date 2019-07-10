#pragma once
#include <editor/Editor.h>

#include "ImGuizmo.h"

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
	uint32_t m_selectedInstanceIdx = 0xFFFFFFFF;

	editor::ImGuiWindowHandle m_windowHandle;

	ImGuizmo::OPERATION m_gizmoOp = ImGuizmo::TRANSLATE;
	ImGuizmo::MODE m_gizmoMode = ImGuizmo::WORLD;

	gfx::Camera m_lockedCam;
	bool m_lockFrustum = false;

};

}