#include "Tonemapper.h"
#include "PostProcessPass.h"
#include "RenderTexture.h"
#include "CBVBuffer.h"
#include <pix3.h>

Tonemapper::Tonemapper(RenderContext& context, const std::shared_ptr<CBVBuffer<PostProcessSettings>>& settings)
	: m_context(context), m_settings(settings)
{
	m_tonemappingPass = std::make_unique<PostProcessPass>(m_context, "shaders/tonemapping_pass.slang", "Tonemapping");
}

void Tonemapper::Tonemap(ID3D12GraphicsCommandList4* commandList, RenderTexture& input, RenderTexture& output, uint32_t backBufferIndex) const
{
	PIXScopedEvent(commandList, 0x6bfa8c, "Tonemapping");

	input.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	output.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	PostProcessPass::PostProcessBindings bindings;
	bindings.inputSrv = input.GetSRV().gpuHandle;
	bindings.outputUav = output.GetUAV().gpuHandle;
	bindings.cbvs[0] = m_settings->GetGPUAddress(backBufferIndex);
	bindings.cbvCount = 1;
	bindings.width = output.GetWidth();
	bindings.height = output.GetHeight();

	m_tonemappingPass->Dispatch(commandList, bindings);
}
