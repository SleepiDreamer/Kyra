#pragma once
#include "CommonDX.h"
#include <memory>
#include <string>
#include <wrl.h>

class Shader;
class ShaderCompiler;
class CommandQueue;

class PostProcessPass
{
public:
    PostProcessPass(RenderContext& context, ShaderCompiler& compiler, const std::string& shaderPath, const std::string& entryPoint);
    ~PostProcessPass();
    PostProcessPass(const PostProcessPass&) = delete;
    PostProcessPass& operator=(const PostProcessPass&) = delete;

    struct PostProcessBindings
    {
        D3D12_GPU_DESCRIPTOR_HANDLE inputSrv;
        D3D12_GPU_DESCRIPTOR_HANDLE outputUav;
        D3D12_GPU_VIRTUAL_ADDRESS constants[4] = {};
        uint32_t constantCount = 0;
        uint32_t width;
        uint32_t height;
    };

    void Dispatch(ID3D12GraphicsCommandList4* commandList, const PostProcessBindings& bindings) const;

    bool CheckHotReload(CommandQueue& commandQueue);
    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] std::string GetLastCompileError() const;

private:
    void BuildRootSignature();
    void BuildPSO();

    RenderContext& m_context;
    std::unique_ptr<Shader> m_shader;
    std::string m_entryPoint;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

    static constexpr uint32_t THREAD_GROUP_SIZE = 8;
};