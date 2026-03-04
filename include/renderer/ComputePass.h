#pragma once
#include "CommonDX.h"
#include <memory>
#include <string>
#include <wrl.h>

class Shader;
class ShaderCompiler;
class CommandQueue;
class RootSignature;

class ComputePass
{
public:
    ComputePass(RenderContext& context, ShaderCompiler& compiler, RootSignature& rootSignature,
        const std::string& shaderPath, const std::string& entryPoint, uint32_t threadGroupSizeX = 8, 
        uint32_t threadGroupSizeY = 8, uint32_t threadGroupSizeZ = 1);
    ~ComputePass();
    ComputePass(const ComputePass&) = delete;
    ComputePass& operator=(const ComputePass&) = delete;

    void Bind(ID3D12GraphicsCommandList4* commandList) const;
    void Dispatch(ID3D12GraphicsCommandList4* commandList, uint32_t elementsX, uint32_t elementsY = 1, uint32_t elementsZ = 1) const;

    bool CheckHotReload(CommandQueue& commandQueue);
    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] std::string GetLastCompileError() const;

private:
    void BuildPSO();

    RenderContext& m_context;
    RootSignature& m_rootSignature;
    std::unique_ptr<Shader> m_shader;
    std::string m_entryPoint;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

    uint32_t m_threadGroupSizeX;
    uint32_t m_threadGroupSizeY;
    uint32_t m_threadGroupSizeZ;
};