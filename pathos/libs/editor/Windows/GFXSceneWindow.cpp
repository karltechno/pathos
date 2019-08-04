#include <kt/Strings.h>

#include <gfx/Scene.h>
#include <gfx/Camera.h>
#include <gfx/Material.h>
#include <gfx/Model.h>
#include <gfx/DebugRender.h>
#include <gfx/ResourceManager.h>

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
	kt::Slice<gfx::Model> models = gfx::ResourceManager::GetAllModels();
	for (uint32_t i = 0; i < models.Size(); ++i)
	{
		ImGui::PushID(i);
		gfx::Model const& model = models[i];
		char const* name = model.m_name.c_str();
		if (ImGui::CollapsingHeader(name))
		{
			ImGui::Text("Sub Meshes: %u", model.m_meshes.Size());
			ImGui::Text("Bounding Box Min: x: %.2f, y: %.2f, z: %.2f", model.m_boundingBox.m_min[0], model.m_boundingBox.m_min[1], model.m_boundingBox.m_min[2]);
			ImGui::Text("Bounding Box Max: x: %.2f, y: %.2f, z: %.2f", model.m_boundingBox.m_max[0], model.m_boundingBox.m_max[1], model.m_boundingBox.m_max[2]);

			if (ImGui::Button("Add instance"))
			{
				gfx::Scene::ModelInstance& instance = _window->m_scene->m_modelInstances.PushBack();
				instance.m_modelIdx = gfx::ResourceManager::ModelIdx(uint16_t(i));
				instance.m_mtx = kt::Mat4::Identity();
				_window->m_selectedInstanceIdx = _window->m_scene->m_modelInstances.Size() - 1;
			}
		}
		ImGui::PopID();
	}
}

static void DrawInstancesTab(GFXSceneWindow* _window)
{
	ImGui::Text("Total Model Instances: %u", _window->m_scene->m_modelInstances.Size());
	static bool s_drawsSceneBounds = false;
	ImGui::Checkbox("Draw Scene Bounds", &s_drawsSceneBounds); 

	if (s_drawsSceneBounds)
	{
		gfx::DebugRender::LineBox(_window->m_scene->m_sceneBounds, kt::Mat3::Identity(), kt::Vec3(0.0f), kt::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
	}

	ImGui::Separator();
	ImGui::Columns(2);

	kt::Array<gfx::Scene::ModelInstance>& instanceArray = _window->m_scene->m_modelInstances;
	for (uint32_t i = 0; i < instanceArray.Size(); ++i)
	{
		ImGui::PushID(i);
		gfx::Model const& model = *gfx::ResourceManager::GetModel(instanceArray[i].m_modelIdx);
		char const* modelName = model.m_name.c_str();
		if (ImGui::Selectable(modelName, i == _window->m_selectedInstanceIdx))
		{
			_window->m_selectedInstanceIdx = i;
		}
		ImGui::PopID();
	}

	ImGui::NextColumn();

	if (_window->m_selectedInstanceIdx < _window->m_scene->m_modelInstances.Size())
	{
		gfx::Scene::ModelInstance& instance = instanceArray[_window->m_selectedInstanceIdx];
		gfx::Model const& model = *gfx::ResourceManager::GetModel(instance.m_modelIdx);
		gfx::DebugRender::LineBox(model.m_boundingBox, instance.m_mtx, kt::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

		ImGuizmo::Manipulate(_window->m_cam->GetView().Data(), _window->m_cam->GetProjection().Data(), _window->m_gizmoOp, _window->m_gizmoMode, instance.m_mtx.Data(), false);
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

static void DrawMaterialsTab(GFXSceneWindow* _window)
{
	ImGui::Columns(2);

	kt::Slice<gfx::Material> materials = gfx::ResourceManager::GetAllMaterials();

	for (uint32_t i = 0; i < materials.Size(); ++i)
	{
		ImGui::PushID(i);
		if (ImGui::Selectable(materials[i].m_name.Data(), _window->m_selectedMaterialidx == i))
		{
			_window->m_selectedMaterialidx = i;
		}
		ImGui::PopID();
	}

	ImGui::NextColumn();

	if (_window->m_selectedMaterialidx < materials.Size())
	{
		bool edited = false;
		gfx::Material& mat = materials[_window->m_selectedMaterialidx];
		edited |= ImGui::ColorEdit4("Base Colour", (float*)&mat.m_params.m_baseColour);
		edited |= ImGui::SliderFloat("Roughness", &mat.m_params.m_roughnessFactor, 0.0f, 1.0f);
		edited |= ImGui::SliderFloat("Metallic", &mat.m_params.m_metallicFactor, 0.0f, 1.0f);
		edited |= ImGui::SliderFloat("Alpha Cutoff", &mat.m_params.m_alphaCutoff, 0.0f, 1.0f);
		if (edited)
		{
			gfx::ResourceManager::SetMaterialsDirty();
		}
	}
	else
	{
		ImGui::Text("No material selected");
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

	ImGui::ColorEdit3("Sun Color", &m_scene->m_sunColor[0]);
	ImGui::DragFloat("Sun Intensity", &m_scene->m_sunIntensity, 1.0f, 0.05f, 1000.0f, "%.3f", 7.0f);

	ImGui::SliderFloat("Sun Theta", &m_scene->m_sunThetaPhi.x, 0.0f, kt::kPi * 2.0f);
	ImGui::SliderFloat("Sun Phi", &m_scene->m_sunThetaPhi.y, -kt::kPiOverTwo, kt::kPiOverTwo);

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

		if (ImGui::BeginTabItem("Materials"))
		{
			DrawMaterialsTab(this);
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