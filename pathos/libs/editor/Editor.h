#pragma once
#include <kt/Handles.h>
#include <kt/StaticFunction.h>

namespace input
{
struct Event;
}

namespace editor
{

struct ImGuiWindowHandle { kt::VersionedHandle hndl; };

using ImGuiWindowUpdateFn = kt::StaticFunction<void(float _dt), 32>;
using ImGuiWindowSetupFn = kt::StaticFunction<void(), 32>;

void Init(void* _nwh);
void Shutdown();

void BeginFrame(float _dt);
void Draw(float _dt);
void EndFrame();

bool HandleInputEvent(input::Event const& _event);

ImGuiWindowHandle RegisterWindow(char const* _group, char const* _name, ImGuiWindowUpdateFn&& _updateFn, ImGuiWindowSetupFn&& _setupFn = ImGuiWindowSetupFn{});
void UnregisterWindow(ImGuiWindowHandle _hndl);

}