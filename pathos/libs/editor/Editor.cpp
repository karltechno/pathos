#include "Editor.h"

#include <core/CVar.h>
#include <gpu/GPUDevice.h>
#include <input/InputTypes.h>

#include <kt/Strings.h>
#include <kt/Array.h>

#include "imgui.h"
#include "ImGuiHandler.h"

namespace editor
{

struct WindowData
{
	kt::String64 m_name;
	ImGuiWindowSetupFn m_setupFn;
	ImGuiWindowUpdateFn m_updateFn;
	bool m_open = false;
};

struct WindowGroup
{
	kt::String64 m_groupName;
	kt::Array<ImGuiWindowHandle> m_handles;
};

struct Context
{
	ImGuiHandler m_imgui;

	kt::VersionedHandlePool<WindowData> m_windowHandles;
	kt::Array<WindowGroup> m_windowGroups;

	bool m_openDemoWindow = false;
	bool m_openAboutWindow = false;
	bool m_openMetricsWindow = false;

	// todo: hide and snap to middle.
	bool m_lockKbMouse = false;
} s_ctx;


void Init(void* _nwh)
{
	s_ctx.m_windowHandles.Init(kt::GetDefaultAllocator(), 256);
	s_ctx.m_imgui.Init(_nwh);
}

void Shutdown()
{
	s_ctx.m_imgui.Shutdown();
}

void DrawUserWindowBar()
{
	for (WindowGroup& grp : s_ctx.m_windowGroups)
	{
		if (ImGui::BeginMenu(grp.m_groupName.Data()))
		{
			for (kt::Array<ImGuiWindowHandle>::Iterator it = grp.m_handles.Begin();
				 it != grp.m_handles.End();
				 /* */)
			{
				if (WindowData* data = s_ctx.m_windowHandles.Lookup((*it).hndl))
				{
					ImGui::MenuItem(data->m_name.Data(), nullptr, &data->m_open);
					++it;
				}
				else
				{
					// This was deleted.
					it = grp.m_handles.EraseSwap(it);
				}
			}
			ImGui::EndMenu();
		}
	}
}

void DrawActiveUserWindows(float _dt)
{
	for (WindowData& data : s_ctx.m_windowHandles)
	{
		if (data.m_open)
		{
			if (data.m_setupFn)
			{
				data.m_setupFn();
			}

			if (ImGui::Begin(data.m_name.Data(), &data.m_open))
			{
				data.m_updateFn(_dt);
			}
			ImGui::End();
		}
	}
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
		ImGui::MenuItem("About", nullptr, &s_ctx.m_openAboutWindow);
		ImGui::MenuItem("Demo", nullptr, &s_ctx.m_openDemoWindow);
		ImGui::MenuItem("Metrics", nullptr, &s_ctx.m_openMetricsWindow);

		if (ImGui::BeginMenu("Styles"))
		{
			ImGui::ShowStyleSelector("##");
			ImGui::EndMenu();
		}

		ImGui::EndMenu();
	}

	core::DrawImGuiCVarMenuItems();

	DrawUserWindowBar();
	DrawActiveUserWindows(_dt);

	ImGui::EndMainMenuBar();
}

void BeginFrame(float _dt)
{
	s_ctx.m_imgui.BeginFrame(_dt);
}

void Draw(float _dt)
{
	DoEditorImGui(_dt);
}

void EndFrame()
{
	s_ctx.m_imgui.EndFrame();
}

bool HandleInputEvent(input::Event const& _event)
{
	if (s_ctx.m_imgui.HandleInputEvent(_event))
	{
		return true;
	}

	switch (_event.m_type)
	{
		case input::Event::Type::KeyDown:
		{
			if (_event.m_key == input::Key::Tilde)
			{
				s_ctx.m_lockKbMouse = !s_ctx.m_lockKbMouse;
				return true;
			}

			return s_ctx.m_lockKbMouse;
		} break;

		case input::Event::Type::KeyUp:
		case input::Event::Type::MouseMove:
		{
			return s_ctx.m_lockKbMouse;
		} break;
	}

	return false;
}

ImGuiWindowHandle RegisterWindow(char const* _group, char const* _name, ImGuiWindowUpdateFn&& _updateFn, ImGuiWindowSetupFn&& _setupFn)
{
	WindowData* data;
	ImGuiWindowHandle handle = { s_ctx.m_windowHandles.Alloc(data) };
	data->m_name = kt::String64(_name);
	data->m_updateFn = std::move(_updateFn);
	data->m_setupFn = std::move(_setupFn);

	for (WindowGroup& grp : s_ctx.m_windowGroups)
	{
		if (kt::StrCmpI(grp.m_groupName.Data(), _group) == 0)
		{
			// todo: check for dupes
			grp.m_handles.PushBack(handle);
			return handle;
		}
	}

	// new group
	WindowGroup& grp = s_ctx.m_windowGroups.PushBack();
	grp.m_groupName = kt::String64(_group);
	grp.m_handles.PushBack(handle);
	return handle;
}

void UnregisterWindow(ImGuiWindowHandle _hndl)
{
	s_ctx.m_windowHandles.Free(_hndl.hndl);
}

}