#include <stdio.h>

#include <input/InputTypes.h>
#include <input/Input.h>
#include <gfx/Scene.h>
#include <gpu/GPUDevice.h>
#include <gpu/CommandContext.h>

#include <kt/Mat4.h>

#include "ImGuiHandler.h"

#include "imgui.h"
#include "ImGuizmo.h"


namespace editor
{

void ImGuiHandler::Init(void* _nwh)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	// This seems to make some lines almost disappear (eg checkbox) - bug or wrong config somewhere ?
	ImGui::GetStyle().AntiAliasedLines = false;

	io.ImeWindowHandle = _nwh;
	io.BackendPlatformName = "pathos";
	io.BackendRendererName = "pathos_d3d12";

	io.KeyMap[ImGuiKey_Tab] = uint32_t(input::Key::Tab);
	io.KeyMap[ImGuiKey_LeftArrow] = uint32_t(input::Key::Left);
	io.KeyMap[ImGuiKey_RightArrow] = uint32_t(input::Key::Right);
	io.KeyMap[ImGuiKey_UpArrow] = uint32_t(input::Key::Up);
	io.KeyMap[ImGuiKey_DownArrow] = uint32_t(input::Key::Down);
	io.KeyMap[ImGuiKey_PageUp] = uint32_t(input::Key::PageUp);
	io.KeyMap[ImGuiKey_PageDown] = uint32_t(input::Key::PageDown);
	io.KeyMap[ImGuiKey_Home] = uint32_t(input::Key::Home);;
	io.KeyMap[ImGuiKey_End] = uint32_t(input::Key::End);
	io.KeyMap[ImGuiKey_Insert] = uint32_t(input::Key::Insert);
	io.KeyMap[ImGuiKey_Delete] = uint32_t(input::Key::Delete);
	io.KeyMap[ImGuiKey_Backspace] = uint32_t(input::Key::BackSpace);
	io.KeyMap[ImGuiKey_Space] = uint32_t(input::Key::Space);
	io.KeyMap[ImGuiKey_Enter] = uint32_t(input::Key::Enter);
	io.KeyMap[ImGuiKey_Escape] = uint32_t(input::Key::Escape);
	io.KeyMap[ImGuiKey_A] = uint32_t(input::Key::KeyA);
	io.KeyMap[ImGuiKey_C] = uint32_t(input::Key::KeyC);
	io.KeyMap[ImGuiKey_V] = uint32_t(input::Key::KeyV);
	io.KeyMap[ImGuiKey_X] = uint32_t(input::Key::KeyX);
	io.KeyMap[ImGuiKey_Y] = uint32_t(input::Key::KeyY);
	io.KeyMap[ImGuiKey_Z] = uint32_t(input::Key::KeyZ);

	gpu::ShaderRef const pixelShader = gfx::ResourceManager::LoadShader("shaders/ImGui.ps.cso", gpu::ShaderType::Pixel);
	gpu::ShaderRef const vertexShader = gfx::ResourceManager::LoadShader("shaders/ImGui.vs.cso", gpu::ShaderType::Vertex);

	gpu::GraphicsPSODesc psoDesc;

	psoDesc.m_vs = vertexShader;
	psoDesc.m_ps = pixelShader;
	psoDesc.m_rasterDesc.m_frontFaceCCW = 0;

	psoDesc.m_vertexLayout
		.Add(gpu::Format::R32G32_Float, gpu::VertexSemantic::Position, false)
		.Add(gpu::Format::R32G32_Float, gpu::VertexSemantic::TexCoord, false)
		.Add(gpu::Format::R8G8B8A8_UNorm, gpu::VertexSemantic::Color, false);

	psoDesc.m_renderTargetFormats[0] = gpu::BackbufferFormat();
	psoDesc.m_numRenderTargets = 1;

	psoDesc.m_blendDesc.m_blendEnable = true;
	psoDesc.m_blendDesc.m_blendOp = gpu::BlendOp::Add;
	psoDesc.m_blendDesc.m_blendOpAlpha = gpu::BlendOp::Add;
	psoDesc.m_blendDesc.m_destAlpha = gpu::BlendMode::Zero;
	psoDesc.m_blendDesc.m_destBlend = gpu::BlendMode::InvSrcAlpha;
	psoDesc.m_blendDesc.m_srcAlpha = gpu::BlendMode::InvSrcAlpha;
	psoDesc.m_blendDesc.m_srcBlend = gpu::BlendMode::SrcAlpha;

	psoDesc.m_depthStencilDesc.m_depthEnable = false;
	psoDesc.m_depthStencilDesc.m_depthWrite = false;
	psoDesc.m_depthStencilDesc.m_stencilEnable = false;

	m_pso.AcquireNoRef(gpu::CreateGraphicsPSO(psoDesc));

	uint8_t* texels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&texels, &width, &height);

	gpu::TextureDesc const texDesc = gpu::TextureDesc::Desc2D(width, height, gpu::TextureUsageFlags::ShaderResource, gpu::Format::R8G8B8A8_UNorm);
	m_fontTex = gpu::CreateTexture(texDesc, texels, "ImGui Font");

	io.Fonts->TexID = ImTextureID(size_t(m_fontTex.Handle().m_packed));

	gpu::BufferDesc vertexBufferDesc;
	vertexBufferDesc.m_flags = gpu::BufferFlags::Transient | gpu::BufferFlags::Vertex;
	vertexBufferDesc.m_strideInBytes = sizeof(ImDrawVert);
	vertexBufferDesc.m_sizeInBytes = 0;
	m_vtxBuf = gpu::CreateBuffer(vertexBufferDesc, nullptr, "ImGui Vtx Buffer");

	gpu::BufferDesc indexBufferDesc;
	indexBufferDesc.m_flags = gpu::BufferFlags::Index | gpu::BufferFlags::Transient;
	indexBufferDesc.m_strideInBytes = sizeof(ImDrawIdx);
	indexBufferDesc.m_format = sizeof(ImDrawIdx) == 2 ? gpu::Format::R16_Uint : gpu::Format::R32_Uint;
	m_idxBuf = gpu::CreateBuffer(indexBufferDesc, nullptr, "ImGui Idx Buffer");
}

void ImGuiHandler::Shutdown()
{
	m_idxBuf.Reset();
	m_vtxBuf.Reset();
	m_fontTex.Reset();
	m_pso.Reset();
}

bool ImGuiHandler::HandleInputEvent(input::Event const& _event)
{
	ImGuiIO& io = ImGui::GetIO();
	switch (_event.m_type)
	{
		//case input::Event::Type::GamepadUp:
		//case input::Event::Type::GamepadDown:
		//{
		//} break;

		case input::Event::Type::TextInput:
		{
			if (!io.WantTextInput)
			{
				return false;
			}
			io.AddInputCharactersUTF8(_event.m_stringUtf8);
		} break;

		case input::Event::Type::MouseWheelDelta:
		{
			io.MouseWheel += _event.m_wheelDelta > 0 ? 1.0f : -1.0f;
		} break;

		case input::Event::Type::KeyUp:
		case input::Event::Type::KeyDown:
		{
			bool const isDown = _event.m_type == input::Event::Type::KeyDown;
			io.KeysDown[uint32_t(_event.m_key)] = isDown;

			switch (_event.m_key)
			{
				case input::Key::LeftControl:
				{
					io.KeyCtrl = isDown;
				} break;

				case input::Key::LeftAlt:
				{
					io.KeyAlt = isDown;
				} break;

				case input::Key::LeftShift:
				{
					io.KeyShift = isDown;
				} break;

				default: {} break;
			}

			return io.WantCaptureKeyboard;
		} break;

		case input::Event::Type::MouseMove:
		{
			return io.WantCaptureMouse;
		} break;

		case input::Event::Type::MouseButtonDown:
		case input::Event::Type::MouseButtonUp:
		{
			bool const isDown = _event.m_type == input::Event::Type::MouseButtonDown;

			switch (_event.m_mouseButton)
			{
				case input::MouseButton::Left:
				{
					io.MouseDown[0] = isDown;
				} break;

				case input::MouseButton::Right:
				{
					io.MouseDown[1] = isDown;

				} break;

				case input::MouseButton::Middle:
				{
					io.MouseDown[2] = isDown;
				} break;

				default:
				{
				} break;
			}

			return io.WantCaptureMouse;
		} break;
	
		default:
		{
			return false;
		} break;
	}

	return false;
}

void ImGuiHandler::BeginFrame(float _dt)
{
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = _dt;

	uint32_t w, h;
	gpu::GetSwapchainDimensions(w, h);
	io.DisplaySize = ImVec2(float(w), float(h));
	
	int32_t mouse[2];
	input::GetCursorPos(mouse[0], mouse[1]);

	io.MousePos = ImVec2(float(mouse[0]), float(mouse[1]));

	ImGui::NewFrame();
	ImGuizmo::BeginFrame();
}

void ImGuiHandler::EndFrame()
{
	ImGui::EndFrame();
	ImGui::Render();
	InternalRender();
}

void ImGuiHandler::InternalRender()
{
	ImDrawData* drawData = ImGui::GetDrawData();

	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();

	GPU_PROFILE_SCOPE(ctx, "ImGui::Render", GPU_PROFILE_COLOUR(0x00, 0x00, 0xfe));

	gpu::cmd::SetRenderTarget(ctx, 0, gpu::CurrentBackbuffer());

	uint32_t const vertAllocSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
	uint32_t const idxAllocSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

	ImDrawIdx* idxDest = (ImDrawIdx*)gpu::cmd::BeginUpdateTransientBuffer(ctx, m_idxBuf, idxAllocSize).Data();
	ImDrawVert* vtxDest = (ImDrawVert*)gpu::cmd::BeginUpdateTransientBuffer(ctx, m_vtxBuf, vertAllocSize).Data();
	for (int i = 0; i < drawData->CmdListsCount; ++i)
	{
		ImDrawList const* drawList = drawData->CmdLists[i];
		memcpy(idxDest, drawList->IdxBuffer.Data, drawList->IdxBuffer.Size * sizeof(ImDrawIdx));
		memcpy(vtxDest, drawList->VtxBuffer.Data, drawList->VtxBuffer.Size * sizeof(ImDrawVert));
		idxDest += drawList->IdxBuffer.Size;
		vtxDest += drawList->VtxBuffer.Size;
	}
	gpu::cmd::EndUpdateTransientBuffer(ctx, m_idxBuf);
	gpu::cmd::EndUpdateTransientBuffer(ctx, m_vtxBuf);

	kt::Mat4 const mtx = kt::Mat4::OrthographicRH_ZO(drawData->DisplayPos.x,
												  drawData->DisplayPos.x + drawData->DisplaySize.x,
												  drawData->DisplayPos.y + drawData->DisplaySize.y,
												  drawData->DisplayPos.y,
												  0.0f, 1.0f);
	
	gpu::DescriptorData cbv;
	cbv.Set(&mtx, sizeof(mtx));
	gpu::cmd::SetGraphicsCBVTable(ctx, cbv, 0);

	gpu::Rect viewport;
	viewport.m_bottomRight = kt::Vec2(drawData->DisplaySize.x, drawData->DisplaySize.y);
	viewport.m_topLeft = kt::Vec2(0.0f);
	gpu::cmd::SetViewport(ctx, viewport, 0.0f, 1.0f);
	gpu::cmd::SetPSO(ctx, m_pso);
	gpu::cmd::SetVertexBuffer(ctx, 0, m_vtxBuf);
	gpu::cmd::SetIndexBuffer(ctx, m_idxBuf);

	uint32_t vtxOffs = 0;
	uint32_t idxOffs = 0;
	kt::Vec2 const clipOffs = kt::Vec2(drawData->DisplayPos.x, drawData->DisplayPos.y);

	gpu::TextureHandle texHandle;

	for (int i = 0; i < drawData->CmdListsCount; ++i)
	{
		ImDrawList const* list = drawData->CmdLists[i];

		for (int cmdIdx = 0; cmdIdx < list->CmdBuffer.Size; ++cmdIdx)
		{
			ImDrawCmd const* cmd = list->CmdBuffer.Data + cmdIdx;
			// TODO: User callback
			gpu::TextureHandle newTexHandle;
			newTexHandle.m_packed = uint32_t(uintptr_t(cmd->TextureId));

			if (newTexHandle != texHandle)
			{
				gpu::DescriptorData srv;
				srv.Set(newTexHandle);
				gpu::cmd::SetGraphicsSRVTable(ctx, srv, 0);
				texHandle = newTexHandle;
			}

			gpu::Rect rect;
			rect.m_topLeft = kt::Vec2(cmd->ClipRect.x, cmd->ClipRect.y) - clipOffs;
			rect.m_bottomRight = kt::Vec2(cmd->ClipRect.z, cmd->ClipRect.w) - clipOffs;
			gpu::cmd::SetScissorRect(ctx, rect);
			gpu::cmd::DrawIndexedInstanced(ctx, cmd->ElemCount, 1, idxOffs, vtxOffs, 0);

			idxOffs += cmd->ElemCount;
		}
		vtxOffs += list->VtxBuffer.Size;
	}
}

}