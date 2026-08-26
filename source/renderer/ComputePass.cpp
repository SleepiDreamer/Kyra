#include "ComputePass.h"
#include "ShaderCompiler.h"
#include "Shader.h"
#include "CommandQueue.h"
#include "Log.h"

#include <d3dx12.h>

using namespace Microsoft::WRL;

ComputePass::ComputePass(RenderContext& context, const std::string& shaderPath,
    const std::string& entryPoint, std::string name, const std::optional<D3D12_STATIC_SAMPLER_DESC>& customSampler)
    : m_name(std::move(name)), m_context(context), m_entryPoint(entryPoint), m_customSampler(customSampler)
{
    m_shader = std::make_unique<Shader>(*m_context.shaderCompiler, shaderPath, std::vector<std::string>{ entryPoint }, false);

    if (m_shader->IsValid())
    {
        Log::Info("Compiled shader: {}", shaderPath);
    }
    else
    {
        Log::Critical("[{}] Failed to compile shader: {}", m_name, shaderPath);
    }

    BuildRootSignature();
    BuildPSO();

    // Shader hot reload callback
    m_context.shaderCompiler->RegisterShaderReload(m_shader.get());
    m_context.shaderCompiler->ShaderRecompileCallback([this](const Shader* shader)
        {
            if (shader == m_shader.get())
            {
                m_context.commandQueue->Flush();
                m_pso.Reset();
                BuildPSO();
            }
        });
}

ComputePass::~ComputePass() = default;

void ComputePass::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER1 params[CONSTANTS_OFFSET + 1] = {};
    for (uint32_t i = 0; i < MAX_SRVS; i++)
    {
        params[SRV_OFFSET + i].InitAsShaderResourceView(i, 0); // t0+:0
    }

    for (uint32_t i = 0; i < MAX_UAVS; i++)
    {
        params[UAV_OFFSET + i].InitAsUnorderedAccessView(i, 0); // u0+:0
    }

    for (uint32_t i = 0; i < MAX_CBVS; i++)
    {
        params[CBV_OFFSET + i].InitAsConstantBufferView(i, 0); // b0+:0
    }

    params[CONSTANTS_OFFSET].InitAsConstants(MAX_CONSTANTS, 0, 1); // b0:1

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
    desc.Init_1_1(std::size(params), params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED);

    ComPtr<ID3DBlob> sigBlob, errorBlob;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &sigBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            const char* message = static_cast<const char*>(errorBlob->GetBufferPointer());
            Log::Critical("{}", message);
        }
        ThrowIfFailed(hr);
    }

    ThrowIfFailed(m_context.device->CreateRootSignature(0, sigBlob->GetBufferPointer(),
        sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    m_rootSignature->SetName(L"Compute Pass Root Signature");
}

void ComputePass::BuildPSO()
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = m_rootSignature.Get();
    desc.CS = m_shader->GetBytecode();

    ThrowIfFailed(m_context.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_pso)));
    m_pso->SetName(L"Compute Pass PSO");
}

void ComputePass::Dispatch(ID3D12GraphicsCommandList4* commandList, const ComputeBindings& bindings) const
{
    commandList->SetComputeRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(m_pso.Get());

    for (uint32_t i = 0; i < bindings.srvCount; i++)
    {
        commandList->SetComputeRootShaderResourceView(SRV_OFFSET + i, bindings.srvs[i]);
    }
    for (uint32_t i = 0; i < bindings.uavCount; i++)
    {
        commandList->SetComputeRootUnorderedAccessView(UAV_OFFSET + i, bindings.uavs[i]);
    }
    for (uint32_t i = 0; i < bindings.cbvCount; i++)
    {
        commandList->SetComputeRootConstantBufferView(CBV_OFFSET + i, bindings.cbvs[i]);
    }
    if (bindings.rootConstantCount > 0)
    {
        commandList->SetComputeRoot32BitConstants(CONSTANTS_OFFSET, bindings.rootConstantCount, bindings.rootConstants, 0);
    }

    uint32_t groupsX = (bindings.threads.x + bindings.threadGroupSize.x - 1) / bindings.threadGroupSize.x;
	uint32_t groupsY = (bindings.threads.y + bindings.threadGroupSize.y - 1) / bindings.threadGroupSize.y;
	uint32_t groupsZ = (bindings.threads.z + bindings.threadGroupSize.z - 1) / bindings.threadGroupSize.z;
	if (groupsX == 0) Log::Error("[{}] Dispatch group count X is 0. Threads: {}, ThreadGroupSize: {}", m_name, bindings.threads.x, bindings.threadGroupSize.x);
	if (groupsY == 0) Log::Error("[{}] Dispatch group count Y is 0. Threads: {}, ThreadGroupSize: {}", m_name, bindings.threads.y, bindings.threadGroupSize.y);
	if (groupsZ == 0) Log::Error("[{}] Dispatch group count Z is 0. Threads: {}, ThreadGroupSize: {}", m_name, bindings.threads.z, bindings.threadGroupSize.z);
    commandList->Dispatch(groupsX, groupsY, groupsZ);
}