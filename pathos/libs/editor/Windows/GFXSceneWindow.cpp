#include <kt/Strings.h>

#include <gfx/Scene.h>
#include <gfx/Camera.h>
#include <gfx/Model.h>
#include <gfx/DebugRender.h>

#include "GFXSceneWindow.h"

#include "imgui.h"
#include "ImGuizmo.h"

namespace editor
{



GFXSceneWindow::GFXSceneWindow()
{
	m_windowHandle = editor::RegisterWindow("gfx", "Scene", [this](float _dt) { Draw(_dt); }, []() { ImGui::SetNextWindowSize(ImVec2(600.0f, 550.0f), ImGuiCond_Once); });
}

GFXSceneWindow::~GFXSceneWindow()
{
	editor::UnregisterWindow(m_windowHandle);
}

static void DrawModelsTab(GFXSceneWindow* _window)
{
	for (uint32_t i = 0; i < gfx::Scene::s_models.Size(); ++i)
	{
		ImGui::PushID(i);
		char const* name = gfx::Scene::s_modelNames[i].Data();
		if (ImGui::CollapsingHeader(name))
		{
			gfx::Model const& model = *gfx::Scene::s_models[i];
			ImGui::Text("Sub Meshes: %u", model.m_meshes.Size());
			ImGui::Text("Bounding Box Min: x: %.2f, y: %.2f, z: %.2f", model.m_boundingBox.m_min[0], model.m_boundingBox.m_min[1], model.m_boundingBox.m_min[2]);
			ImGui::Text("Bounding Box Max: x: %.2f, y: %.2f, z: %.2f", model.m_boundingBox.m_max[0], model.m_boundingBox.m_max[1], model.m_boundingBox.m_max[2]);
			ImGui::Text("Num Materials: %u", model.m_materials.Size());

			if (ImGui::Button("Add instance"))
			{
				gfx::Scene::InstanceData& instance = _window->m_scene->m_instances.PushBack();
				instance.m_modelIdx = i;
				instance.m_transform.m_pos = kt::Vec3(0.0f);
				instance.m_transform.m_mtx = kt::Mat3::Identity();
				_window->m_selectedInstanceIdx = _window->m_scene->m_instances.Size() - 1;
			}
		}
		ImGui::PopID();
	}
}

static void DrawInstancesTab(GFXSceneWindow* _window)
{
	ImGui::Text("Total Instances: %u", _window->m_scene->m_instances.Size());
	static bool s_drawsSceneBounds = false;
	ImGui::Checkbox("Draw Scene Bounds", &s_drawsSceneBounds); 

	if (s_drawsSceneBounds)
	{
		gfx::DebugRender::LineBox(_window->m_scene->m_sceneBounds, kt::Mat3::Identity(), kt::Vec3(0.0f), kt::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
	}

	ImGui::Separator();
	ImGui::Columns(2);

	kt::Array<gfx::Scene::InstanceData>& instanceArray = _window->m_scene->m_instances;
	for (uint32_t i = 0; i < instanceArray.Size(); ++i)
	{
		ImGui::PushID(i);
		char const* modelName = gfx::Scene::s_modelNames[instanceArray[i].m_modelIdx].Data();
		if (ImGui::Selectable(modelName, i == _window->m_selectedInstanceIdx))
		{
			_window->m_selectedInstanceIdx = i;
		}
		ImGui::PopID();
	}

	ImGui::NextColumn();

	if (_window->m_selectedInstanceIdx < _window->m_scene->m_instances.Size())
	{
		gfx::Scene::InstanceData& instance = instanceArray[_window->m_selectedInstanceIdx];
		gfx::Model const& model = *gfx::Scene::s_models[instance.m_modelIdx];
		gfx::DebugRender::LineBox(model.m_boundingBox, instance.m_transform.m_mtx, instance.m_transform.m_pos, kt::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

		kt::Mat4 mtx;

		mtx.m_cols[0] = kt::Vec4(instance.m_transform.m_mtx[0], 0.0f);
		mtx.m_cols[1] = kt::Vec4(instance.m_transform.m_mtx[1], 0.0f);
		mtx.m_cols[2] = kt::Vec4(instance.m_transform.m_mtx[2], 0.0f);
		mtx.m_cols[3] = kt::Vec4(instance.m_transform.m_pos, 1.0f);

		ImGuizmo::Manipulate(_window->m_cam->GetView().Data(), _window->m_cam->GetProjection().Data(), _window->m_gizmoOp, _window->m_gizmoMode, mtx.Data(), false);

		instance.m_transform.m_mtx[0] = kt::Vec3(mtx.m_cols[0]);
		instance.m_transform.m_mtx[1] = kt::Vec3(mtx.m_cols[1]);
		instance.m_transform.m_mtx[2] = kt::Vec3(mtx.m_cols[2]);
		instance.m_transform.m_pos = kt::Vec3(mtx.m_cols[3]);
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
		ImGui::SliderFloat("Intensity", &light.intensity, 1.0f, 10000.0f, "%.3f", 2.0f);
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

	if (ImGui::Checkbox("Lock camera", &m_lockFrustum))
	{
		m_lockedCam = *m_cam;
	}

	if (m_lockFrustum)
	{
		gfx::DebugRender::LineFrustum(m_lockedCam, kt::Vec4(0.0f, 1.0f, 1.0f, 1.0f));
	}

	ImGui::ColorEdit3("Sun Color", &m_scene->m_frameConstants.sunColor[0]);
	ImGui::SliderFloat3("Sun Dir", &m_scene->m_frameConstants.sunDir[0], -1.0f, 1.0f);
	m_scene->m_frameConstants.sunDir = kt::Normalize(m_scene->m_frameConstants.sunDir);

	char const* const modes[] = { "Local", "World" };
	char const* const ops[] = { "Translate", "Rotate", "Scale" };

	ImGui::ListBox("Gizmo Mode", (int*)&m_gizmoMode, modes, KT_ARRAY_COUNT(modes));
	ImGui::ListBox("Gizmo Op", (int*)&m_gizmoOp, ops, KT_ARRAY_COUNT(ops));

	ImGuizmo::Enable(true);

	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);

	if (ImGui::BeginTabBar("SceneTabs"))
	{
		if (ImGui::BeginTabItem("Models"))
		{
			DrawModelsTab(this);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Instances"))
		{
			DrawInstancesTab(this);
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