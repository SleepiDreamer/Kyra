#include "PostProcessPass.h"
#include "ShaderCompiler.h"
#include "Shader.h"
#include "CommandQueue.h"
#include "Log.h"

#include <d3dx12.h>

using namespace Microsoft::WRL;

PostProcessPass::PostProcessPass(RenderContext& context, ShaderCompiler& compiler, const std::string& shaderPath, 
								 const std::string& entryPoint, const std::optional<D3D12_STATIC_SAMPLER_DESC>& customSampler)
    : m_context(context), m_entryPoint(entryPoint), m_customSampler(customSampler)
{
    m_shader = std::make_unique<Shader>(compiler, shaderPath, std::vector<std::string>{ entryPoint }, false);

    if (m_shader->IsValid())
    {
		Log::Info("Compiled shader: {}", shaderPath);
    }
    else
    {
        Log::Critical("Failed to compile shader: {}", shaderPath);
    }

    BuildRootSignature();
    BuildPSO();

    // Shader hot reload callback
    compiler.RegisterShaderReload(m_shader.get());
    compiler.ShaderRecompileCallback([this](const Shader* shader)
    {
        if (shader == m_shader.get())
        {
            m_context.commandQueue->Flush();
            m_pso.Reset();
            BuildPSO();
        }
    });
}

PostProcessPass::~PostProcessPass() = default;

void PostProcessPass::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE1 srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

    CD3DX12_DESCRIPTOR_RANGE1 uavRange;
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

    CD3DX12_ROOT_PARAMETER1 params[7] = {};
    params[0].InitAsDescriptorTable(1, &srvRange);  // t0:0
    params[1].InitAsDescriptorTable(1, &uavRange);  // u0:0
    params[2].InitAsConstantBufferView(0, 0);       // b0:0
    params[3].InitAsConstantBufferView(1, 0);       // b1:0
    params[4].InitAsConstantBufferView(2, 0);       // b2:0
    params[5].InitAsConstantBufferView(3, 0);       // b3:0
	params[6].InitAsConstants(16, 4, 0);            // b4:0

    D3D12_STATIC_SAMPLER_DESC sampler;
    if (m_customSampler.has_value())
    {
        sampler = m_customSampler.value();
    }
    else
    {
        sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MaxAnisotropy = 16;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
    desc.Init_1_1(std::size(params), params, 1, &sampler);

    ComPtr<ID3DBlob> sigBlob, errorBlob;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &sigBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
            std::cerr << static_cast<const char*>(errorBlob->GetBufferPointer()) << "\n";
        ThrowIfFailed(hr);
    }

    ThrowIfFailed(m_context.device->CreateRootSignature(0, sigBlob->GetBufferPointer(),
        sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    m_rootSignature->SetName(L"Post-Process Root Signature");
}

void PostProcessPass::BuildPSO()
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = m_rootSignature.Get();
    desc.CS = m_shader->GetBytecode();

    ThrowIfFailed(m_context.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_pso)));
    m_pso->SetName(L"Post-Process PSO");
}

void PostProcessPass::Dispatch(ID3D12GraphicsCommandList4* commandList, const PostProcessBindings& bindings) const
{
    commandList->SetComputeRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(m_pso.Get());

    commandList->SetComputeRootDescriptorTable(0, bindings.inputSrv);
    commandList->SetComputeRootDescriptorTable(1, bindings.outputUav);
    for (uint32_t i = 0; i < bindings.constantBufferCount; i++)
    {
	    commandList->SetComputeRootConstantBufferView(2 + i, bindings.constantBuffers[i]);
    }
    if (bindings.rootConstantCount > 0)
    {
        commandList->SetComputeRoot32BitConstants(6, bindings.rootConstantCount, bindings.rootConstants, 0);
    }

    uint32_t groupsX = (bindings.width + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
    uint32_t groupsY = (bindings.height + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
    commandList->Dispatch(groupsX, groupsY, 1);
}