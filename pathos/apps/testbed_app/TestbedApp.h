#pragma once
#include <app/App.h>
#include <app/CameraController.h>

#include <gpu/Types.h>
#include <gpu/HandleRef.h>

#include <gfx/Camera.h>
#include <gfx/Resources.h>
#include <gfx/Model.h>
#include <gfx/Scene.h>
#include <gfx/Primitive.h>

#include <res/Resource.h>
#include <res/ResourceSystem.h>

#include <editor/Windows/GFXSceneWindow.h>
#include <shaderlib/LightingStructs.h>


struct TestbedApp : app::GraphicsApp
{
	void Setup() override;
	void Tick(float _dt) override;
	void Shutdown() override;

	void HandleInputEvent(input::Event const& _ev) override;

	struct DummyCbuffer
	{
		kt::Mat4 mvp;
	};

	DummyCbuffer m_myCbuffer;

	shaderlib::TestLightCBuffer m_testLightCbufferData;
	gpu::BufferRef m_lightCbuffer;

	editor::GFXSceneWindow m_sceneWindow;
	gfx::Scene m_scene;

	gfx::Camera m_cam;
	app::CameraController m_camController;

	gpu::PSORef m_csPso;
	gpu::PSORef m_pso;
	gpu::PSORef m_irradPso;

	gpu::BufferRef m_constantBuffer;
	gpu::TextureRef m_cubeMap;
	gpu::TextureRef m_irradMap;

	res::ResourceHandle<gfx::ShaderResource> m_pixelShader;
	res::ResourceHandle<gfx::ShaderResource> m_vertexShader;
	res::ResourceHandle<gfx::Model> m_modelHandle;

	gpu::PSORef m_equiPso;
	gfx::Texture m_equiTex;

	gpu::PSORef m_skyBoxPso;

	gfx::PrimitiveGPUBuffers m_cubeData;

};

