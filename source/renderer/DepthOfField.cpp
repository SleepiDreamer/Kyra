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
	m_outputTexture = std::make_unique<RenderTexture>(m_context, DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, L"DoF Output Texture");
}

DepthOfField::~DepthOfField() = default;

void DepthOfField::Resize(const uint32_t width, const uint32_t height) const
{
	m_nearFieldTexture->Resize(m_context.device, width, height);
	m_farFieldTexture->Resize(m_context.device, width, height);
	m_outputTexture->Resize(m_context.device, width, height);
}

void DepthOfField::Render(ID3D12GraphicsCommandList4* commandList, const DepthOfFieldInput& inputs, uint32_t backBufferIndex) const
{
	inputs.color->Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	inputs.depth->Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	m_nearFieldTexture->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_farFieldTexture->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_outputTexture->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	PostProcessPass::PostProcessBindings bindings;
	bindings.cbvs[0] = m_settings->GetGPUAddress(backBufferIndex);
	bindings.cbvCount = 1;
	bindings.rootConstants[0] = inputs.color->GetSRV().index; 		// color
	bindings.rootConstants[1] = inputs.depth->GetSRV().index;  		// depth
	bindings.rootConstants[2] = m_nearFieldTexture->GetUAV().index; // near field
	bindings.rootConstants[3] = m_farFieldTexture->GetUAV().index;  // far field
	bindings.rootConstants[4] = m_outputTexture->GetUAV().index;    // output
	bindings.rootConstantCount = 5;
	bindings.width  = m_outputTexture->GetWidth();
	bindings.height = m_outputTexture->GetHeight();

	m_fieldPass->Dispatch(commandList, bindings);

	m_outputTexture->Transition(commandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	inputs.color->Transition(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyResource(inputs.color->GetResource(), m_outputTexture->GetResource());
}
