#include "ComputePass.h"
#include "ShaderCompiler.h"
#include "Shader.h"
#include "CommandQueue.h"
#include "RootSignature.h"

#include <d3dx12.h>
#include <iostream>

ComputePass::ComputePass(RenderContext& context, ShaderCompiler& compiler, RootSignature& rootSignature, const std::string& shaderPath, 
    const std::string& entryPoint, const uint32_t threadGroupSizeX, const uint32_t threadGroupSizeY, const uint32_t threadGroupSizeZ)
    : m_context(context), m_rootSignature(rootSignature), m_entryPoint(entryPoint)
	, m_threadGroupSizeX(threadGroupSizeX), m_threadGroupSizeY(threadGroupSizeY), m_threadGroupSizeZ(threadGroupSizeZ)
{
    std::vector<std::pair<std::string, std::string>> defines = {};
    m_shader = std::make_unique<Shader>(compiler, shaderPath, std::vector<std::string>{ entryPoint }, defines, false);
    if (!m_shader->IsValid())
    {
        ThrowError("Failed to compile compute shader: " + shaderPath);
    }

    BuildPSO();
}

ComputePass::~ComputePass() = default;

void ComputePass::BuildPSO()
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = m_rootSignature.Get();
    desc.CS = m_shader->GetBytecode();

    ThrowIfFailed(m_context.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_pso)));
    m_pso->SetName(ToWideString(m_entryPoint + " PSO").c_str());
}

void ComputePass::Bind(ID3D12GraphicsCommandList4* commandList) const
{
    commandList->SetComputeRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(m_pso.Get());
}

void ComputePass::Dispatch(ID3D12GraphicsCommandList4* commandList, const uint32_t elementsX, const uint32_t elementsY, const uint32_t elementsZ) const
{
    uint32_t groupsX = (elementsX + m_threadGroupSizeX - 1) / m_threadGroupSizeX;
    uint32_t groupsY = (elementsY + m_threadGroupSizeY - 1) / m_threadGroupSizeY;
    uint32_t groupsZ = (elementsZ + m_threadGroupSizeZ - 1) / m_threadGroupSizeZ;
    commandList->Dispatch(groupsX, groupsY, groupsZ);
}

bool ComputePass::CheckHotReload(CommandQueue& commandQueue)
{
    if (!m_shader->NeedsReload())
        return false;

    if (!m_shader->Reload())
    {
        std::cerr << "[ComputePass] Hot reload failed for " << m_entryPoint << ", keeping previous shader\n";
        return false;
    }

    commandQueue.Flush();
    m_pso.Reset();
    BuildPSO();
    std::cout << "[ComputePass] Hot reload successful for " << m_entryPoint << "\n";
    return true;
}

bool ComputePass::IsValid() const
{
    return m_shader && !m_shader->LastCompileFailed();
}

std::string ComputePass::GetLastCompileError() const
{
    return m_shader ? m_shader->GetLastCompileError() : "";
}