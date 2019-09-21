#include "GPUWindows.h"
#include "Editor.h"

#include "imgui.h"
#include "gpu/GPUDevice.h"
#include "gpu/GPUProfiler.h"
#include "kt/Strings.h"

namespace editor
{

void GPUWindows::Register()
{
	m_resourceWindow = editor::RegisterWindow("gpu", "resources", [this](float _dt) { DrawResources(_dt); }, []() { ImGui::SetNextWindowSize(ImVec2(400.0f, 400.0f), ImGuiCond_FirstUseEver); });
	m_profilerWindow = editor::RegisterWindow("gpu", "Profiler", [this](float _dt) {DrawProfiler(_dt); }, []() { ImGui::SetNextWindowSize(ImVec2(600.0f, 500.0f), ImGuiSetCond_FirstUseEver); });

}

void GPUWindows::Unregister()
{
	editor::UnregisterWindow(m_resourceWindow);
	editor::UnregisterWindow(m_profilerWindow);
}

void GPUWindows::DrawBufferTab()
{
	ImGui::Columns(2);

	ImGui::BeginChild("Buffer List");

	gpu::EnumResourceHandles([this](gpu::ResourceHandle _handle)
	{
		gpu::ResourceType type;
		gpu::BufferDesc desc;
		char const* bufferName;
		gpu::GetResourceInfo(_handle, type, &desc, nullptr, &bufferName);

		if (gpu::IsTexture(type))
		{
			return;
		}

		bool selected = _handle == m_selectedBuffer;
		ImGui::PushID(int(_handle.m_packed));
		if (ImGui::Selectable(bufferName, selected))
		{
			m_selectedBuffer = gpu::BufferHandle{ _handle };
		}

		ImGui::PopID();
	});

	ImGui::EndChild();

	ImGui::NextColumn();

	gpu::ResourceType type;
	gpu::BufferDesc desc;
	char const* bufferName;

	if (!gpu::GetResourceInfo(m_selectedBuffer, type, &desc, nullptr, &bufferName))
	{
		ImGui::Text("No selected buffer.");
		m_selectedBuffer = gpu::BufferHandle{};
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
}

void GPUWindows::DrawTextureTab()
{
	ImGui::Columns(2);

	ImGui::BeginChild("Texture List");

	gpu::EnumResourceHandles([this](gpu::ResourceHandle _handle)
	{
		gpu::ResourceType type;
		gpu::TextureDesc desc;
		char const* texName;
		gpu::GetResourceInfo(_handle, type, nullptr, &desc, &texName);

		if (!gpu::IsTexture(type))
		{
			return;
		}

		bool selected = _handle == m_selectedTexture;
		ImGui::PushID(int(_handle.m_packed));
		if (ImGui::Selectable(texName, selected))
		{
			m_selectedTexture = gpu::TextureHandle{ _handle };
		}

		ImGui::PopID();
	});

	ImGui::EndChild();

	ImGui::NextColumn();

	gpu::ResourceType type;
	gpu::TextureDesc desc;
	char const* texName;

	if (!gpu::GetResourceInfo(m_selectedTexture, type, nullptr, &desc, &texName))
	{
		ImGui::Text("No selected texture.");
		m_selectedTexture = gpu::TextureHandle{};
	}
	else
	{

		ImGui::Text("Name: %s", texName);
		ImGui::Text("Width: %u", desc.m_width);
		ImGui::Text("Height: %u", desc.m_height);
		ImGui::Text("Format: %s", gpu::GetFormatName(desc.m_format));

		if (!!(desc.m_usageFlags & gpu::TextureUsageFlags::ShaderResource))
		{
			ImTextureID texId = ImTextureID(uintptr_t(m_selectedTexture.m_packed));
			float const ratio = ImGui::GetContentRegionAvailWidth() / desc.m_width;
			ImGui::Image(texId, ImVec2(desc.m_width * ratio, desc.m_height * ratio));
		}
		else
		{
			ImGui::TextWrapped("Texture can't be previewed (wasn't created with shader resource flag).");
		}
	}

	ImGui::Columns();
}

void GPUWindows::DrawResources(float _dt)
{
	KT_UNUSED(_dt);
	ImGui::BeginTabBar("Resource Types");

	if (ImGui::BeginTabItem("Buffers"))
	{
		DrawBufferTab();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Textures"))
	{
		DrawTextureTab();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

static void DrawProfilerRecursive(gpu::profiler::ResolvedTreeEntry const* _array, uint32_t _idx, double _timeRange, uint64_t _timeBegin, uint64_t _ticksPerSecond, uint32_t _depth)
{
	float width = ImGui::GetContentRegionAvailWidth();

	ImVec2 const windowPos = ImGui::GetCursorScreenPos();
	gpu::profiler::ResolvedTreeEntry const& entry = _array[_idx];
	ImDrawList* list = ImGui::GetWindowDrawList();

	float const c_profileHeight = 25.0f;

	float const xBegin = width * float(double(entry.beginTime - _timeBegin) / _timeRange);
	float const xEnd = xBegin + width * float(double(entry.endTime - entry.beginTime) / _timeRange);

	float const yBegin = c_profileHeight * _depth + 1.0f;
	float const yEnd = c_profileHeight * (_depth + 1) - 1.0f;

	ImVec2 const rectTL = ImVec2(windowPos.x + xBegin, windowPos.y + yBegin);
	ImVec2 const rectBR = ImVec2(windowPos.x + xEnd, windowPos.y + yEnd);

	kt::String512 tooltip;
	double const ms = (double(entry.endTime - entry.beginTime) * 1000.0) / _ticksPerSecond;
	tooltip.AppendFmt("%s - %.2fms", entry.name, ms);

	ImVec2 const textSize = ImGui::CalcTextSize(tooltip.Begin(), tooltip.End());

	list->AddRectFilled(rectTL, rectBR, entry.colour, 0.0f);

	ImGui::PushClipRect(rectTL, rectBR, true);

	float const textBeginLocalX = (rectBR.x - rectTL.x) * 0.5f - textSize.x * 0.5f;
	float const textBeginLocalY = (rectBR.y - rectTL.y) * 0.5f - textSize.y * 0.5f;

	list->AddText(ImVec2(rectTL.x + textBeginLocalX, rectTL.y + textBeginLocalY), ~entry.colour | 0xff000000, tooltip.Begin(), tooltip.End());

	ImGui::PopClipRect();

	if (ImGui::IsMouseHoveringRect(rectTL, rectBR))
	{
		ImGui::SetTooltip("%s - %.2fms", entry.name, ms);
	}

	if (entry.childLink != UINT32_MAX)
	{
		DrawProfilerRecursive(_array, entry.childLink, _timeRange, _timeBegin, _ticksPerSecond, _depth + 1);
	}

	if (entry.siblingLink != UINT32_MAX)
	{
		DrawProfilerRecursive(_array, entry.siblingLink, _timeRange, _timeBegin, _ticksPerSecond, _depth);
	}
}

void GPUWindows::DrawProfiler(float _dt)
{
	KT_UNUSED(_dt);
	gpu::profiler::ResolvedTreeEntry const* root;
	uint32_t numNodes;
	gpu::profiler::GetFrameTree(&root, &numNodes);

	if (!numNodes)
	{
		return;
	}

	uint64_t const ticksPerSecond = gpu::GetQueryFrequency();

	uint64_t const begin = root->beginTime;

	double const range = root->endTime - root->beginTime;

	DrawProfilerRecursive(root, 0, range, begin, ticksPerSecond, 0);
}



}