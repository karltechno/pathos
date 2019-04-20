#include "Editor.h"

#include <core/CVar.h>

#include "imgui.h"

namespace editor
{

struct Context
{
	
};

void Init()
{
	
}

void Shutdown()
{

}

void Update(float _dt)
{
	KT_UNUSED(_dt);
	ImGui::BeginMainMenuBar();
	core::DrawImGuiCVarMenuItems();
	ImGui::EndMainMenuBar();
}

}