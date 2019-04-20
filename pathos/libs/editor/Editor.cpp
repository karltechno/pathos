#include "Editor.h"

#include <core/CVar.h>
#include <gpu/GPUDevice.h>

#include "imgui.h"

namespace editor
{

struct Context
{
	bool m_openDemoWindow = false;
	bool m_openAboutWindow = false;
	bool m_openMetricsWindow = false;

	bool m_openGpuResourceWindow = false;

	gpu::BufferHandle m_selectedBuffer = gpu::BufferHandle{};
} s_ctx;


void Init()
{
	
}

void Shutdown()
{

}

void GpuResourceWindow()
{
	ImGui::SetNextWindowSize(ImVec2(400.0f, 400.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("GPU Resources", &s_ctx.m_openGpuResourceWindow);

	ImGui::BeginTabBar("Resource Types");

	if (ImGui::BeginTabItem("Buffers"))
	{
		ImGui::Columns(2);

		ImGui::BeginChild("Buffer List");

		gpu::EnumBufferHandles([](gpu::BufferHandle _handle)
		{
			gpu::BufferDesc desc;
			char const* bufferName;
			gpu::GetBufferInfo(_handle, desc, bufferName);
		
			bool selected = _handle == s_ctx.m_selectedBuffer;
			ImGui::PushID(int(_handle.m_packed));
			if (ImGui::Selectable(bufferName, selected))
			{
				s_ctx.m_selectedBuffer = _handle;
			}

			ImGui::PopID();
		});

		ImGui::EndChild();

		ImGui::NextColumn();

		gpu::BufferDesc desc;
		char const* bufferName;
		if (!gpu::GetBufferInfo(s_ctx.m_selectedBuffer, desc, bufferName))
		{
			ImGui::Text("No selected buffer.");
			s_ctx.m_selectedBuffer = gpu::BufferHandle{};
		}
		else
		{

			ImGui::Text("Name: %s", bufferName);
			ImGui::Text("Size: %u", desc.m_sizeInBytes);
			ImGui::Text("Stride: %u", desc.m_strideInBytes);
			ImGui::Text("Format: %s", gpu::GetFormatName(desc.m_format));
			ImGui::Text("Flags: ");

			bool didAny = false;

			auto writeFlag = [&desc, &didAny](gpu::BufferFlags _flag, char const* _text)
			{
				if (!!(desc.m_flags & _flag))
				{
					ImGui::SameLine();
					if (didAny) { ImGui::Text("|"); }
					ImGui::SameLine();
					ImGui::Text("%s", _text);
					didAny = true;
				}
			};
			writeFlag(gpu::BufferFlags::Constant, "Constant");
			writeFlag(gpu::BufferFlags::Vertex, "Vertex");
			writeFlag(gpu::BufferFlags::Index, "Index");
			writeFlag(gpu::BufferFlags::UnorderedAccess, "UAV");
			writeFlag(gpu::BufferFlags::ShaderResource, "SRV");
			writeFlag(gpu::BufferFlags::Transient, "Transient");

		}

		ImGui::Columns();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
	ImGui::End();
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

	if (s_ctx.m_openGpuResourceWindow)
	{
		GpuResourceWindow();
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

	if (ImGui::BeginMenu("gpu"))
	{
		ImGui::MenuItem("Resources", nullptr, &s_ctx.m_openGpuResourceWindow);
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