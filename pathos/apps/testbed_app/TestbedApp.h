#pragma once
#include <app/App.h>
#include <app/CameraController.h>
#include <gpu/Types.h>
#include <gfx/Camera.h>
#include <gfx/Resources.h>
#include <res/Resource.h>
#include <res/ResourceSystem.h>
#include <gfx/Model.h>

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
	gpu::BufferHandle m_constantBuffer;

	res::ResourceHandle<gfx::ShaderResource> m_pixelShader;
	res::ResourceHandle<gfx::ShaderResource> m_vertexShader;
	res::ResourceHandle<gfx::Model> m_modelHandle;
};

