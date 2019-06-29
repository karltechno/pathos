#include <kt/Strings.h>

#include <gfx/Scene.h>
#include <gfx/Camera.h>

#include "GFXSceneWindow.h"

#include "imgui.h"
#include "ImGuizmo.h"

namespace editor
{



GFXSceneWindow::GFXSceneWindow()
{
	m_windowHandle = editor::RegisterWindow("gfx", "Scene", [this](float _dt) {Draw(_dt); }, []() { ImGui::SetNextWindowSize(ImVec2(600.0f, 550.0f), ImGuiCond_Once); });
}

GFXSceneWindow::~GFXSceneWindow()
{
	editor::UnregisterWindow(m_windowHandle);
}

static void DrawModelsTab(GFXSceneWindow* _window)
{
	KT_UNUSED(_window);
	ImGui::Columns(2);
	ImGui::Text("Models");
	ImGui::NextColumn();
	ImGui::Text("Instances");
	ImGui::NextColumn();
	ImGui::Separator();

	for (uint32_t i = 0; i < gfx::Scene::s_models.Size(); ++i)
	{
		ImGui::PushID(i);
		char const* name = gfx::Scene::s_modelNames[i].Data();
		if (ImGui::CollapsingHeader(name))
		{
			if (ImGui::Button("Add instance"))
			{
				gfx::Scene::Instance& instance = _window->m_scene->m_instances.PushBack();
				instance.m_modelIdx = i;
				instance.m_pos = kt::Vec3(0.0f);
				instance.m_rot = kt::Mat3::Identity();
			}
		}
		ImGui::PopID();
	}

	ImGui::NextColumn();

	for (gfx::Scene::Instance& instance : _window->m_scene->m_instances)
	{
		//gfx::Model* m = gfx::Scene::s_models[instance.m_modelIdx];
		char const* name = gfx::Scene::s_modelNames[instance.m_modelIdx].Data();

		ImGui::PushID(&instance);
		if (ImGui::CollapsingHeader(name))
		{
			ImGui::DragFloat3("Pos", &instance.m_pos[0]);
		}
		ImGui::PopID();
	}

	ImGui::Columns();
}

static void DrawLightsTab(GFXSceneWindow* _window)
{
	ImGui::Columns(2);
	if (ImGui::Button("Add Light"))
	{
		shaderlib::LightData& light = _window->m_scene->m_lights.PushBack();
		light.color = kt::Vec3(1.0f, 0.0f, 0.0f);
		light.direction = kt::Vec3(1.0f, 0.0f, 0.0f);
		light.posWS = kt::Vec3(0.0f);
		light.rcpRadius = 1.0f / 20.0f;
		_window->m_selectedLightIdx = _window->m_scene->m_lights.Size() - 1;
	}

	ImGui::Separator();
	for (uint32_t i = 0; i < _window->m_scene->m_lights.Size(); ++i)
	{
		ImGui::PushID(i);
		kt::String128 name;
		name.AppendFmt("Light %u", i + 1);
		if (ImGui::Selectable(name.Data(), _window->m_selectedLightIdx == i))
		{
			_window->m_selectedLightIdx = i;
		}
		ImGui::PopID();
	}

	ImGui::NextColumn();

	if (_window->m_selectedLightIdx < _window->m_scene->m_lights.Size())
	{
		shaderlib::LightData& light = _window->m_scene->m_lights[_window->m_selectedLightIdx];
		ImGui::ColorEdit3("Color", &light.color[0]);
		ImGui::SliderFloat3("Direction", &light.direction[0], -1.0f, 1.0f);
		light.direction = kt::Normalize(light.direction);
		ImGui::DragFloat3("Pos", &light.posWS[0]);
		float rad = 1.0f / light.rcpRadius;
		ImGui::SliderFloat("Radius", &rad, 1.0f, 1000.0f);
		ImGui::SliderFloat("Intensity", &light.intensity, 1.0f, 100000.0f);
		light.rcpRadius = 1.0f / rad;

		if (ImGui::Button("Remove"))
		{
			_window->m_scene->m_lights.Erase(_window->m_selectedLightIdx);
		}

		if (_window->m_cam)
		{
			kt::Mat4 mtx = kt::Mat4::Identity();
			mtx.SetPos(light.posWS);
			ImGuizmo::Manipulate(_window->m_cam->GetView().Data(), _window->m_cam->GetProjection().Data(), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::LOCAL, mtx.Data());
			light.posWS = kt::Vec3(mtx.m_cols[3].x, mtx.m_cols[3].y, mtx.m_cols[3].z);
		}
	}
	else
	{
		ImGui::Text("No light selected");
	}

	ImGui::Columns();
}

void GFXSceneWindow::Draw(float _dt)
{
	KT_UNUSED(_dt);
	if (!m_scene)
	{
		ImGui::Text("No scene set.");
		return;
	}

	ImGui::ColorEdit3("Sun Color", &m_scene->m_frameConstants.sunColor[0]);
	ImGui::SliderFloat3("Sun Dir", &m_scene->m_frameConstants.sunDir[0], -1.0f, 1.0f);
	m_scene->m_frameConstants.sunDir = kt::Normalize(m_scene->m_frameConstants.sunDir);

	ImGui::Checkbox("Enable Gizmo", &m_guizmoEnabled);
	
	ImGuizmo::Enable(m_guizmoEnabled);
	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);

	if (ImGui::BeginTabBar("SceneTabs"))
	{
		if (ImGui::BeginTabItem("Models"))
		{
			DrawModelsTab(this);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Lights"))
		{
			DrawLightsTab(this);
			ImGui::EndTabItem();
		}


		ImGui::EndTabBar();
	}

}

void GFXSceneWindow::SetScene(gfx::Scene* _scene)
{
	m_scene = _scene;
}


void GFXSceneWindow::SetMainViewCamera(gfx::Camera* _cam)
{
	m_cam = _cam;
}

}