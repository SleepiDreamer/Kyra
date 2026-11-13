#include "DepthOfField.h"
#include "PostProcessPass.h"
#include "RenderTexture.h"
#include "Texture.h"
#include "CBVBuffer.h"
#include "CubeParser.h"
#include <pix3.h>

DepthOfField::DepthOfField(RenderContext& context, const uint32_t width, const uint32_t height, const std::shared_ptr<CBVBuffer<PostProcessSettings>>& settings)
	: m_context(context), m_settings(settings)
{
	m_fieldPass = std::make_unique<PostProcessPass>(m_context, "shaders/dof/dof_field_pass.slang", "FieldPass");
	m_downscalePass = std::make_unique<PostProcessPass>(m_context, "shaders/dof/dof_downscale_pass.slang", "DownscalePass");
	m_combinePass = std::make_unique<PostProcessPass>(m_context, "shaders/dof/dof_combine_pass.slang", "CombinePass");

	m_nearFieldTexture = std::make_unique<RenderTexture>(m_context, DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, L"DoF Near Field Texture");
	m_farFieldTexture = std::make_unique<RenderTexture>(m_context, DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, L"DoF Far Field Texture");
}

DepthOfField::~DepthOfField() = default;

void DepthOfField::Resize(const uint32_t width, const uint32_t height) const
{
	m_nearFieldTexture->Resize(m_context.device, width, height);
	m_farFieldTexture->Resize(m_context.device, width, height);
}

void DepthOfField::Render(ID3D12GraphicsCommandList4* commandList, RenderTexture& input, RenderTexture& output, uint32_t backBufferIndex) const
{
	input.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	output.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	PostProcessPass::PostProcessBindings bindings;
	bindings.inputSrv = input.GetSRV().gpuHandle; // placeholder
	bindings.outputUav = output.GetUAV().gpuHandle; // placeholder
	bindings.cbvs[0] = m_settings->GetGPUAddress(backBufferIndex);
	bindings.cbvCount = 1;
	bindings.rootConstants[0] = input.GetSRV().index; // albedo
	bindings.rootConstants[1] = input.GetSRV().index; // depth
	bindings.rootConstants[2] = output.GetUAV().index; // near
	bindings.rootConstants[3] = output.GetUAV().index; // far
	bindings.rootConstantCount = 3;
	bindings.width = output.GetWidth();
	bindings.height = output.GetHeight();

	m_fieldPass->Dispatch(commandList, bindings);
}
