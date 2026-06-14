#pragma once
#include "CommonDX.h"
#include <memory>
#include <string>
#include <wrl.h>

class Shader;
class ShaderCompiler;
class CommandQueue;

class ComputePass
{
public:
    ComputePass(RenderContext& context, const std::string& shaderPath, const std::string& entryPoint, const std::optional<D3D12_STATIC_SAMPLER_DESC>& customSampler = std::nullopt);
    ~ComputePass();
    ComputePass(const ComputePass&) = delete;
    ComputePass& operator=(const ComputePass&) = delete;

    static constexpr uint32_t MAX_SRVS = 4;
    static constexpr uint32_t MAX_UAVS = 4;
    static constexpr uint32_t MAX_CBVS = 4;
    static constexpr uint32_t MAX_CONSTANTS = 16;

    static constexpr uint32_t SRV_OFFSET = 0;
    static constexpr uint32_t UAV_OFFSET = MAX_SRVS;
    static constexpr uint32_t CBV_OFFSET = UAV_OFFSET + MAX_UAVS;
    static constexpr uint32_t CONSTANTS_OFFSET = CBV_OFFSET + MAX_CBVS;

    struct ComputeBindings
    {

        D3D12_GPU_VIRTUAL_ADDRESS srvs[MAX_SRVS] = {};
        uint32_t srvCount = 0;
        D3D12_GPU_VIRTUAL_ADDRESS uavs[MAX_UAVS] = {};
        uint32_t uavCount = 0;
        D3D12_GPU_VIRTUAL_ADDRESS cbvs[MAX_CBVS] = {};
        uint32_t cbvCount = 0;
        uint32_t rootConstants[MAX_CONSTANTS] = {};
        uint32_t rootConstantCount = 0;
        uint32_t threads;
    };

    void Dispatch(ID3D12GraphicsCommandList4* commandList, const ComputeBindings& bindings) const;

private:
    void BuildRootSignature();
    void BuildPSO();

    RenderContext& m_context;
    std::unique_ptr<Shader> m_shader;
    std::string m_entryPoint;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
    std::optional<D3D12_STATIC_SAMPLER_DESC> m_customSampler;

    static constexpr uint32_t THREAD_GROUP_SIZE = 32;
};