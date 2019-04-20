#pragma once
#include <app/App.h>

namespace editor
{

struct TestbedApp : app::GraphicsApp
{
	void Setup() override {}
	void Tick(float const) override {}
	void Shutdown() override {}

	void HandleInputEvent(input::Event const&) override {}
};

}

