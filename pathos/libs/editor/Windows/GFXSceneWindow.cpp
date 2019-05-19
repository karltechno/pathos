#include <gfx/Scene.h>

#include "GFXSceneWindow.h"

#include "imgui.h"

namespace editor
{



GFXSceneWindow::GFXSceneWindow()
{
	m_windowHandle = editor::RegisterWindow("gfx", "Scene", [this](float _dt) {Draw(_dt); });
}

GFXSceneWindow::~GFXSceneWindow()
{
	editor::UnregisterWindow(m_windowHandle);
}

void GFXSceneWindow::Draw(float _dt)
{
	KT_UNUSED(_dt);
	if (!m_scene)
	{
		ImGui::Text("No scene set.");
		return;
	}

	ImGui::ColorEdit3("Sun Color", &m_scene->m_sunColor[0]);
	ImGui::SliderFloat3("Sun Dir", &m_scene->m_sunDir[0], -1.0f, 1.0f);
	m_scene->m_sunDir = kt::Normalize(m_scene->m_sunDir);

	if (ImGui::Button("Add Light"))
	{
		shaderlib::LightData& light = m_scene->m_lights.PushBack();
		light.color = kt::Vec3(1.0f, 0.0f, 0.0f);
		light.direction = kt::Vec3(1.0f, 0.0f, 0.0f);
		light.posWS = kt::Vec3(0.0f);
		light.rcpRadius = 1.0f / 20.0f;
	}

	for (uint32_t i = 0; i < m_scene->m_lights.Size(); ++i)
	{
		ImGui::PushID(i);
		if (ImGui::TreeNode(&m_scene->m_lights[0], "Light %u", i + 1))
		{
			shaderlib::LightData& light = m_scene->m_lights[i];
			ImGui::ColorEdit3("Color", &light.color[0]);
			ImGui::SliderFloat3("Direction", &light.direction[0], -1.0f, 1.0f);
			light.direction = kt::Normalize(light.direction);
			ImGui::DragFloat3("Pos", &light.posWS[0]);
			float rad = 1.0f / light.rcpRadius;
			ImGui::SliderFloat("Radius", &rad, 1.0f, 1000.0f);
			ImGui::SliderFloat("Intensity", &light.intensity, 1.0f, 100000.0f);
			light.rcpRadius = 1.0f / rad;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}

void GFXSceneWindow::SetScene(gfx::Scene* _scene)
{
	m_scene = _scene;
}


}