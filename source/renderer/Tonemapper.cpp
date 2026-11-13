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
	LoadLUT("assets/luts/OpenDRT_SDR.cube", "OpenDRT SDR");
	LoadLUT("assets/luts/OpenDRT_Colorful_SDR.cube", "OpenDRT SDR (Colorful)");
	LoadLUT("assets/luts/OpenDRT_HDR800.cube", "OpenDRT HDR");
	LoadLUT("assets/luts/OpenDRT_Colorful_HDR800.cube", "OpenDRT HDR (Colorful)");
}

Tonemapper::~Tonemapper() = default;

void Tonemapper::LoadLUT(const std::string& path, const std::string& name)
{
	CubeLUT lut;
	std::string error;
	if (!ParseCubeLUT(path, lut, &error))
	{
		Log::Error("Failed to load Cube LUT: {}", error);
	}
	const std::vector<uint16_t> lutData = lut.ToRGBA16F();
	
	m_luts[name] = Texture();
	m_luts.at(name).Create3D(m_context, lutData.data(), lut.size, lut.size, lut.size, 
		DXGI_FORMAT_R16G16B16A16_FLOAT, "Tonemapping LUT 3D Texture");
	
	bool incorrectName = true;
	for (const auto& tonemap : m_tonemapOperatorsSdr)
	{
		if (tonemap == name) incorrectName = false;
	}
	for (const auto& tonemap : m_tonemapOperatorsHdr)
	{
		if (tonemap == name) incorrectName = false;
	}
	if (incorrectName)
	{
		Log::Warning("[Tonemapper::LoadLUT] '{}' is not a valid tonemap operator", name);
	}
}

void Tonemapper::SetActiveTonemapSDR(const std::string& name)
{
	for (int i = 0; i < std::size(m_tonemapOperatorsSdr); i++)
	{
		if (m_tonemapOperatorsSdr[i] == name)
		{
			m_activeTonemapSdr = i;
			return;
		}
	}
	Log::Warning("[Tonemapper::SetActiveTonemapSDR] '{}' is not a valid tonemap operator", name);
}

void Tonemapper::SetActiveTonemapHDR(const std::string& name)
{
	for (int i = 0; i < std::size(m_tonemapOperatorsHdr); i++)
	{
		if (m_tonemapOperatorsHdr[i] == name)
		{
			m_activeTonemapHdr = i;
			return;
		}
	}
	Log::Warning("[Tonemapper::SetActiveTonemapHDR] '{}' is not a valid tonemap operator", name);
}

void Tonemapper::Tonemap(ID3D12GraphicsCommandList4* commandList, RenderTexture& input, RenderTexture& output, uint32_t backBufferIndex, bool hdrEnabled) const
{
	input.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	output.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	
	const uint32_t activeTonemap = hdrEnabled ? m_activeTonemapHdr : m_activeTonemapSdr;
	const std::string& activeTonemapName = hdrEnabled ? 
		m_tonemapOperatorsHdr[activeTonemap] : m_tonemapOperatorsSdr[activeTonemap];
	
	PostProcessPass::PostProcessBindings bindings;
	bindings.inputSrv = input.GetSRV().gpuHandle;
	if (m_luts.contains(activeTonemapName))
	{
		bindings.inputSrv2 = m_luts.at(activeTonemapName).GetSRV().gpuHandle;
	}
	bindings.outputUav = output.GetUAV().gpuHandle;
	bindings.cbvs[0] = m_settings->GetGPUAddress(backBufferIndex);
	bindings.cbvCount = 1;
	bindings.rootConstants[0] = activeTonemap;
	bindings.rootConstantCount = 1;
	bindings.width = output.GetWidth();
	bindings.height = output.GetHeight();

	m_tonemappingPass->Dispatch(commandList, bindings);
}
