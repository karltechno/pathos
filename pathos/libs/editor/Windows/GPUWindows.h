#pragma once
#include <gpu/Types.h>

#include <editor/Editor.h>

namespace editor
{

struct GPUWindows
{
	void Register();
	void Unregister();

	void Draw(float _dt);

	void DrawBufferTab();
	void DrawTextureTab();

	editor::ImGuiWindowHandle m_windowHandle;
	gpu::BufferHandle m_selectedBuffer;
	gpu::TextureHandle m_selectedTexture;
};

}