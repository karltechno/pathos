#include <stdio.h>

#include <input/InputTypes.h>
#include <input/Input.h>

#include <gpu/GPUDevice.h>
#include <gpu/CommandContext.h>

#include <kt/Mat4.h>

#include "imgui.h"
#include "ImGuiHandler.h"


namespace app
{

// TODO: Duplicated/bad
static void DebugReadEntireFile(FILE* _f, gpu::ShaderBytecode& o_byteCode)
{
	fseek(_f, 0, SEEK_END);
	size_t len = ftell(_f);
	fseek(_f, 0, SEEK_SET);
	void* ptr = kt::Malloc(len);
	fread(ptr, len, 1, _f);
	o_byteCode.m_size = len;
	o_byteCode.m_data = ptr;
}


void ImGuiHandler::Init(void* _nwh)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ImeWindowHandle = _nwh;
	io.BackendPlatformName = "pathos_d3d12";

	// io.KeyMap TODO.

	FILE* pshFile = fopen("shaders/ImGui.pixel.cso", "rb");
	FILE* vshFile = fopen("shaders/ImGui.vertex.cso", "rb");
	KT_SCOPE_EXIT(fclose(pshFile); fclose(vshFile));
	gpu::ShaderBytecode vsBytecode, psBytecode;
	DebugReadEntireFile(pshFile, psBytecode);
	DebugReadEntireFile(vshFile, vsBytecode);
	KT_SCOPE_EXIT(kt::Free(vsBytecode.m_data); kt::Free(psBytecode.m_data););

	gpu::ShaderRef vsRef, psRef;
	vsRef.AcquireNoRef(gpu::CreateShader(gpu::ShaderType::Vertex, vsBytecode));
	psRef.AcquireNoRef(gpu::CreateShader(gpu::ShaderType::Pixel, psBytecode));

	gpu::GraphicsPSODesc psoDesc;

	psoDesc.m_vs = vsRef;
	psoDesc.m_ps = psRef;

	psoDesc.m_vertexLayout
		.Add(gpu::Format::R32G32_Float, gpu::VertexSemantic::Position)
		.Add(gpu::Format::R32G32_Float, gpu::VertexSemantic::TexCoord)
		.Add(gpu::Format::R8G8B8A8_UNorm, gpu::VertexSemantic::Color);

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

	gpu::BufferDesc constantBufferDesc;
	constantBufferDesc.m_flags = gpu::BufferFlags::Constant | gpu::BufferFlags::Transient;
	constantBufferDesc.m_sizeInBytes = sizeof(ImGuiCBuffer);
	m_cbuf = gpu::CreateBuffer(constantBufferDesc, nullptr, "ImGui CBuffer");
}

void ImGuiHandler::Shutdown()
{
	m_idxBuf.Reset();
	m_vtxBuf.Reset();
	m_cbuf.Reset();
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

		case input::Event::Type::MouseButtonDown:
		case input::Event::Type::MouseButtonUp:
		{
			if (!io.WantCaptureMouse)
			{
				return false;
			}
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
		} break;
	
		default:
		{
			return false;
		} break;
	}

	return false;
}

void ImGuiHandler::BeginFrame()
{
	ImGuiIO& io = ImGui::GetIO();
	uint32_t w, h;
	gpu::GetSwapchainDimensions(w, h);
	io.DisplaySize = ImVec2(float(w), float(h));
	
	int32_t mouse[2];
	input::GetCursorPos(mouse[0], mouse[1]);

	io.MousePos = ImVec2(float(mouse[0]), float(mouse[1]));

	ImGui::NewFrame();
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

	gpu::cmd::Context* ctx = gpu::cmd::Begin(gpu::cmd::ContextType::Graphics);

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

	ImGuiCBuffer cbuf;
	cbuf.m_orthoMtx = kt::Mat4::OrthographicLH_ZO(drawData->DisplayPos.x,
												  drawData->DisplayPos.x + drawData->DisplaySize.x,
												  drawData->DisplayPos.y + drawData->DisplaySize.y,
												  drawData->DisplayPos.y,
												  0.0f, 1.0f);
	
	gpu::cmd::UpdateTransientBuffer(ctx, m_cbuf, &cbuf);

	gpu::Rect viewport;
	viewport.m_bottomRight = kt::Vec2(drawData->DisplaySize.x, drawData->DisplaySize.y);
	viewport.m_topLeft = kt::Vec2(0.0f);
	gpu::cmd::SetViewport(ctx, viewport, 0.0f, 1.0f);
	gpu::cmd::SetGraphicsPSO(ctx, m_pso);
	gpu::cmd::SetVertexBuffer(ctx, 0, m_vtxBuf);
	gpu::cmd::SetIndexBuffer(ctx, m_idxBuf);
	gpu::cmd::SetConstantBuffer(ctx, m_cbuf, 0, 0);

	uint32_t vtxOffs = 0;
	uint32_t idxOffs = 0;
	kt::Vec2 const clipOffs = kt::Vec2(drawData->DisplayPos.x, drawData->DisplayPos.y);

	for (int i = 0; i < drawData->CmdListsCount; ++i)
	{
		ImDrawList const* list = drawData->CmdLists[i];

		for (int cmdIdx = 0; cmdIdx < list->CmdBuffer.Size; ++cmdIdx)
		{
			ImDrawCmd const* cmd = list->CmdBuffer.Data + cmdIdx;
			// TODO: User callback
			gpu::TextureHandle texHandle;
			texHandle.m_packed = uint32_t(cmd->TextureId);
			gpu::cmd::SetShaderResource(ctx, texHandle, 0, 0);
			gpu::Rect rect;
			rect.m_topLeft = kt::Vec2(cmd->ClipRect.x, cmd->ClipRect.y) - clipOffs;
			rect.m_bottomRight = kt::Vec2(cmd->ClipRect.z, cmd->ClipRect.w) - clipOffs;
			gpu::cmd::SetScissorRect(ctx, rect);
			gpu::cmd::DrawIndexedInstanced(ctx, gpu::PrimitiveType::TriangleList, cmd->ElemCount, 1, idxOffs, vtxOffs, 0);

			idxOffs += cmd->ElemCount;
		}
		vtxOffs += list->VtxBuffer.Size;
	}

	gpu::cmd::End(ctx);
}

}