#include "GPUWindows.h"
#include "Editor.h"

#include "imgui.h"
#include "gpu/GPUDevice.h"

namespace editor
{

void GPUWindows::Register()
{
	m_windowHandle = editor::RegisterWindow("gpu", "resources", [this](float _dt) { Draw(_dt); }, [this]() { ImGui::SetNextWindowSize(ImVec2(400.0f, 400.0f), ImGuiCond_FirstUseEver); });
}

void GPUWindows::Unregister()
{
	editor::UnregisterWindow(m_windowHandle);
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

void GPUWindows::Draw(float _dt)
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

}