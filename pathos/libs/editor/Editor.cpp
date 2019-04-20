#include "Editor.h"

#include <core/CVar.h>

#include "imgui.h"

namespace editor
{

struct Context
{
	bool m_openDemoWindow = false;
	bool m_openAboutWindow = false;
	bool m_openMetricsWindow = false;
} s_ctx;

void Init()
{
	
}

void Shutdown()
{

}

void DoEditorImGui(float _dt)
{
	KT_UNUSED(_dt);

	if (s_ctx.m_openDemoWindow)
	{
		ImGui::ShowDemoWindow(&s_ctx.m_openDemoWindow);
	}

	if (s_ctx.m_openAboutWindow)
	{
		ImGui::ShowAboutWindow(&s_ctx.m_openAboutWindow);
	}

	if (s_ctx.m_openMetricsWindow)
	{
		ImGui::ShowMetricsWindow(&s_ctx.m_openMetricsWindow);
	}

	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("*"))
	{
		if (ImGui::MenuItem("About"))
		{
			s_ctx.m_openAboutWindow = true;
		}

		if (ImGui::MenuItem("Demo"))
		{
			s_ctx.m_openDemoWindow = true;
		}

		if (ImGui::MenuItem("Metrics"))
		{
			s_ctx.m_openMetricsWindow = true;
		}

		if (ImGui::BeginMenu("Styles"))
		{
			ImGui::ShowStyleSelector("##");
			ImGui::EndMenu();
		}

		ImGui::EndMenu();
	}

	core::DrawImGuiCVarMenuItems();

	ImGui::EndMainMenuBar();
}

void Update(float _dt)
{
	DoEditorImGui(_dt);
}

}