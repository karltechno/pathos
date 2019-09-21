#pragma once
#include <gpu/Types.h>

#include <editor/Editor.h>

namespace editor
{

struct GPUWindows
{
	void Register();
	void Unregister();

	void DrawResources(float _dt);
	void DrawProfiler(float _dt);

	void DrawBufferTab();
	void DrawTextureTab();

	editor::ImGuiWindowHandle m_resourceWindow;
	editor::ImGuiWindowHandle m_profilerWindow;

	gpu::BufferHandle m_selectedBuffer;
	gpu::TextureHandle m_selectedTexture;
};

}