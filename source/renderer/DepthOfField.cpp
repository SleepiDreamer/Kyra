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
	D3D12_STATIC_SAMPLER_DESC samplerDesc = POINT_SAMPLER;
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

	m_fieldPass = std::make_unique<PostProcessPass>(m_context, "shaders/dof/dof_field_pass.slang", "FieldPass", samplerDesc);
	m_blurFarPass = std::make_unique<PostProcessPass>(m_context, "shaders/dof/dof_blur_far_pass.slang", "BlurFarPass", samplerDesc);
	m_downscalePass = std::make_unique<PostProcessPass>(m_context, "shaders/dof/dof_downscale_pass.slang", "DownscalePass", samplerDesc);
	m_recombinePass = std::make_unique<PostProcessPass>(m_context, "shaders/dof/dof_combine_pass.slang", "CombinePass", samplerDesc);
	m_debugPass = std::make_unique<PostProcessPass>(m_context, "shaders/dof/dof_debug_pass.slang", "DebugPass", samplerDesc);

	m_nearFieldTexture = std::make_unique<RenderTexture>(m_context, DXGI_FORMAT_R16G16B16A16_FLOAT, width / 2, height / 2, L"DoF Near Field Texture");
	for (int i = 0; i < 3; i++)
	{
		m_nearCoCTextures.emplace_back(std::make_unique<RenderTexture>(m_context, DXGI_FORMAT_R16_UNORM, width / pow(2, i + 1), height / pow(2, i + 1), L"DoF Near Field Downscaled CoC Texture"));
	}
	m_farFieldTexture = std::make_unique<RenderTexture>(m_context, DXGI_FORMAT_R16G16B16A16_FLOAT, width / 2, height / 2, L"DoF Far Field Texture");
	m_blurNearTexture = std::make_unique<RenderTexture>(m_context, DXGI_FORMAT_R16G16B16A16_FLOAT, width / 2, height / 2, L"DoF Blur Near Texture");
	m_blurFarTexture = std::make_unique<RenderTexture>(m_context, DXGI_FORMAT_R16G16B16A16_FLOAT, width / 2, height / 2, L"DoF Blur Far Texture");
	m_outputTexture = std::make_unique<RenderTexture>(m_context, DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, L"DoF Output Texture");
}

DepthOfField::~DepthOfField() = default;

void DepthOfField::Resize(const uint32_t width, const uint32_t height) const
{
	m_nearFieldTexture->Resize(m_context.device, width / 2, height / 2);
	for (size_t i = 0; i < m_nearCoCTextures.size(); i++)
	{
		m_nearCoCTextures[i]->Resize(m_context.device, width / pow(2, i + 1), height / pow(2, i + 1));
	}
	m_farFieldTexture->Resize(m_context.device, width / 2, height / 2);
	m_blurNearTexture->Resize(m_context.device, width / 2, height / 2);
	m_blurFarTexture->Resize(m_context.device, width / 2, height / 2);
	m_outputTexture->Resize(m_context.device, width, height);
}

void DepthOfField::Render(ID3D12GraphicsCommandList4* commandList, const DepthOfFieldInput& inputs, uint32_t backBufferIndex) const
{
	inputs.color->Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	inputs.depth->Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	m_nearFieldTexture->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	for (size_t i = 0; i < m_nearCoCTextures.size(); i++)
	{
		m_nearCoCTextures[i]->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}
	m_farFieldTexture->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_outputTexture->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	PostProcessPass::PostProcessBindings bindings;
	bindings.cbvs[0] = m_settings->GetGPUAddress(backBufferIndex);
	bindings.cbvCount = 1;
	bindings.rootConstants[0] = inputs.color->GetSRV().index; 		 // color
	bindings.rootConstants[1] = inputs.depth->GetSRV().index;  		 // depth
	bindings.rootConstants[2] = m_nearFieldTexture->GetUAV().index;  // near field
	bindings.rootConstants[3] = m_nearCoCTextures[0]->GetUAV().index;// near field coc
	bindings.rootConstants[4] = m_nearCoCTextures[2]->GetUAV().index;// near field coc blur
	bindings.rootConstants[5] = m_farFieldTexture->GetUAV().index;   // far field
	bindings.rootConstants[6] = 0;									 // input
	bindings.rootConstants[7] = 0;									 // output
	bindings.rootConstantCount = 8;
	bindings.width  = m_farFieldTexture->GetWidth();
	bindings.height = m_farFieldTexture->GetHeight();

	// Field extraction
	{
		bindings.rootConstants[7] = m_outputTexture->GetUAV().index; // output
		bindings.width  = m_farFieldTexture->GetWidth();
		bindings.height = m_farFieldTexture->GetHeight();
		m_fieldPass->Dispatch(commandList, bindings);

		D3D12_RESOURCE_BARRIER fieldBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::UAV(m_nearFieldTexture->GetResource()),
			CD3DX12_RESOURCE_BARRIER::UAV(m_nearCoCTextures[0]->GetResource()),
			CD3DX12_RESOURCE_BARRIER::UAV(m_farFieldTexture->GetResource()) };
		commandList->ResourceBarrier(_countof(fieldBarriers), fieldBarriers);
	}

	// Blur far field
	{
		bindings.rootConstants[7] = m_blurFarTexture->GetUAV().index; // output
		bindings.width  = m_blurFarTexture->GetWidth();
		bindings.height = m_blurFarTexture->GetHeight();
		m_blurFarPass->Dispatch(commandList, bindings);

		D3D12_RESOURCE_BARRIER fieldBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::UAV(m_blurFarTexture->GetResource()) };
		commandList->ResourceBarrier(_countof(fieldBarriers), fieldBarriers);
	}

	// Near CoC downsample
	{
		bindings.rootConstants[6] = m_nearCoCTextures[0]->GetSRV().index; // input
		bindings.rootConstants[7] = m_nearCoCTextures[1]->GetUAV().index; // output
		bindings.width  = m_nearCoCTextures[1]->GetWidth();
		bindings.height = m_nearCoCTextures[1]->GetHeight();
		m_downscalePass->Dispatch(commandList, bindings);

		D3D12_RESOURCE_BARRIER downscaleBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::UAV(m_nearCoCTextures[1]->GetResource()) };
		commandList->ResourceBarrier(_countof(downscaleBarriers), downscaleBarriers);

		bindings.rootConstants[6] = m_nearCoCTextures[1]->GetSRV().index; // input
		bindings.rootConstants[7] = m_nearCoCTextures[2]->GetUAV().index; // output
		bindings.width  = m_nearCoCTextures[2]->GetWidth();
		bindings.height = m_nearCoCTextures[2]->GetHeight();
		m_downscalePass->Dispatch(commandList, bindings);

		D3D12_RESOURCE_BARRIER downscaleBarriers2[] = {
			CD3DX12_RESOURCE_BARRIER::UAV(m_nearCoCTextures[2]->GetResource()) };
		commandList->ResourceBarrier(_countof(downscaleBarriers2), downscaleBarriers2);
	}

	// Recombine
	{
		bindings.rootConstants[7] = inputs.color->GetUAV().index; // output
		bindings.width = inputs.color->GetWidth();
		bindings.height = inputs.color->GetHeight();
		m_recombinePass->Dispatch(commandList, bindings);
	}

	// // Copy final texture
	// {
	// 	m_outputTexture->Transition(commandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	// 	inputs.color->Transition(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	// 	commandList->CopyResource(inputs.color->GetResource(), m_outputTexture->GetResource());
	// }

	// Debug output
	if (inputs.settings->dofDebugMode != DOFDebugMode::None)
	{
		PostProcessPass::PostProcessBindings debugBindings;

		switch (inputs.settings->dofDebugMode)
		{
			case DOFDebugMode::Near:
			{
				debugBindings.rootConstants[0] = m_nearFieldTexture->GetSRV().index;
				debugBindings.rootConstants[1] = 0; // channel
				break;
			}
			case DOFDebugMode::NearCoc:
			{
				debugBindings.rootConstants[0] = m_nearCoCTextures[0]->GetSRV().index;
				debugBindings.rootConstants[1] = 2; // channel
				break;
			}
			case DOFDebugMode::NearCocSmall:
			{
				debugBindings.rootConstants[0] = m_nearCoCTextures[2]->GetSRV().index;
				debugBindings.rootConstants[1] = 2; // channel
				break;
			}
			case DOFDebugMode::NearBlur:
			{
				debugBindings.rootConstants[0] = m_blurNearTexture->GetSRV().index;
				debugBindings.rootConstants[1] = 0; // channel
				break;
			}
			case DOFDebugMode::Far:
			{
				debugBindings.rootConstants[0] = m_farFieldTexture->GetSRV().index;
				debugBindings.rootConstants[1] = 0; // channel
				break;
			}
			case DOFDebugMode::FarCoc:
			{
				debugBindings.rootConstants[0] = m_farFieldTexture->GetSRV().index;
				debugBindings.rootConstants[1] = 1; // channel
				break;
			}
			case DOFDebugMode::FarBlur:
			{
				debugBindings.rootConstants[0] = m_blurFarTexture->GetSRV().index;
				debugBindings.rootConstants[1] = 0; // channel
				break;
			}
			case DOFDebugMode::None:
				break;
		}
		debugBindings.cbvs[0] = m_settings->GetGPUAddress(backBufferIndex);
		debugBindings.cbvCount = 1;
		debugBindings.rootConstants[2] = inputs.color->GetUAV().index; // output
		debugBindings.rootConstantCount = 3;
		debugBindings.width  = inputs.color->GetWidth();
		debugBindings.height = inputs.color->GetHeight();
		m_debugPass->Dispatch(commandList, debugBindings);
	}
}
