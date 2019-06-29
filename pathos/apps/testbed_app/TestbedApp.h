#pragma once
#include <app/App.h>
#include <app/CameraController.h>

#include <gpu/Types.h>
#include <gpu/HandleRef.h>

#include <gfx/EnvMap.h>
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

	editor::GFXSceneWindow m_sceneWindow;
	gfx::Scene m_scene;

	gfx::Camera m_cam;
	app::CameraController m_camController;

	gpu::PSORef m_pso;
	gpu::PSORef m_irradPso;

	gpu::BufferRef m_constantBuffer;
	gpu::TextureRef m_cubeMap;
	gpu::TextureRef m_irradMap;
	gpu::TextureRef m_ggxMap;

	gfx::SkyBoxRenderer m_skyboxRenderer;

	res::ResourceHandle<gfx::ShaderResource> m_pixelShader;
	res::ResourceHandle<gfx::ShaderResource> m_vertexShader;
	res::ResourceHandle<gfx::Model> m_modelHandle;
};

