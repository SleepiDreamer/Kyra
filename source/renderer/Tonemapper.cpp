#include "Tonemapper.h"
#include "PostProcessPass.h"
#include "RenderTexture.h"
#include "Texture.h"
#include "CBVBuffer.h"
#include "CubeParser.h"
#include <pix3.h>

Tonemapper::Tonemapper(RenderContext& context, const std::shared_ptr<CBVBuffer<PostProcessSettings>>& settings)
	: m_context(context), m_settings(settings)
{
	m_tonemappingPass = std::make_unique<PostProcessPass>(m_context, "shaders/tonemapping_pass.slang", "Tonemapping");

	m_LUTs.emplace_back(Texture());
	CubeLUT lut;
	std::string error;
	if (!ParseCubeLUT("assets/luts/opendrt_srgb.cube", lut, &error))
	{
		Log::Error("Failed to load Cube LUT: {}", error);
	}
	std::vector<uint16_t> lutData = lut.ToRGBA16F();
	m_LUTs.back().Create3D(m_context, lutData.data(), lut.size, lut.size, lut.size, 
		DXGI_FORMAT_R16G16B16A16_FLOAT, "OpenDRT LUT 3D Texture");
}

Tonemapper::~Tonemapper() = default;

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
