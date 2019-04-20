#pragma once
#include <app/App.h>
#include <gpu/Types.h>

struct TestbedApp : app::GraphicsApp
{
	void Setup() override;
	void Tick(float _dt) override;
	void Shutdown() override;

	void HandleInputEvent(input::Event const&) override {}


	struct KT_ALIGNAS(256) DummyCbuffer
	{
		kt::Vec4 myVec4;
	};

	DummyCbuffer m_myCbuffer;

	gpu::GraphicsPSOHandle m_pso;
	gpu::BufferHandle m_indexBuffer;
	gpu::BufferHandle m_vertexBuffer;
	gpu::BufferHandle m_constantBuffer;

	gpu::ShaderHandle m_pixelShader;
	gpu::ShaderHandle m_vertexShader;
};

