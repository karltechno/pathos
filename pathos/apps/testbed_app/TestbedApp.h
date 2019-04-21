#pragma once
#include <app/App.h>
#include <gpu/Types.h>
#include <gfx/Camera.h>
#include <app/CameraController.h>

struct TestbedApp : app::GraphicsApp
{
	void Setup() override;
	void Tick(float _dt) override;
	void Shutdown() override;

	void HandleInputEvent(input::Event const& _ev) override;


	struct KT_ALIGNAS(256) DummyCbuffer
	{
		kt::Vec4 myVec4;
		kt::Mat4 mvp;
	};

	DummyCbuffer m_myCbuffer;

	gfx::Camera m_cam;
	app::CameraController_Free m_camController;

	gpu::GraphicsPSOHandle m_pso;
	gpu::BufferHandle m_indexBuffer;
	gpu::BufferHandle m_vertexBuffer;
	gpu::BufferHandle m_constantBuffer;

	gpu::ShaderHandle m_pixelShader;
	gpu::ShaderHandle m_vertexShader;
};

