#pragma once
#include <app/App.h>
#include <app/CameraController.h>

#include <gpu/Types.h>
#include <gpu/HandleRef.h>

#include <gfx/EnvMap.h>
#include <gfx/Camera.h>
#include <gfx/Model.h>
#include <gfx/Scene.h>
#include <gfx/Primitive.h>

#include <editor/Windows/GFXSceneWindow.h>

struct TestbedApp : app::GraphicsApp
{
	void Setup() override;
	void Tick(float _dt) override;
	void Shutdown() override;

	void HandleInputEvent(input::Event const& _ev) override;

	editor::GFXSceneWindow m_sceneWindow;
	gfx::Scene m_scene;

	gfx::Camera m_cam;
	app::CameraController m_camController;

	gpu::PSORef m_pso;

	gpu::PSORef m_shadowMapPso;

	gpu::BufferRef m_constantBuffer;
	gpu::TextureRef m_cubeMap;

	gfx::SkyBoxRenderer m_skyboxRenderer;

	gfx::ResourceManager::ModelIdx m_modelIdx;
};

