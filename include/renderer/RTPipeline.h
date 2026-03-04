#pragma once
#include "CommonDX.h"

class ShaderCompiler;
class Shader;
class CommandQueue;
struct HitGroupRecord;

class RTPipeline
{
public:
	RTPipeline(ID3D12Device10* device, ID3D12RootSignature* rootSignature, ShaderCompiler& compiler, 
			   const std::vector<HitGroupRecord>& records, const std::string& shaderPath, 
			   const std::vector<std::pair<std::string, std::string>>& defines = {});
    ~RTPipeline();

    void Rebuild(ID3D12Device10* device, const std::vector<HitGroupRecord>& records);
    void RebuildShaderTables(ID3D12Device10* device, const std::vector<HitGroupRecord>& records);
	bool CheckHotReload(ID3D12Device10* device, CommandQueue& commandQueue, const std::vector<HitGroupRecord>& records);
    
	[[nodiscard]] D3D12_DISPATCH_RAYS_DESC GetDispatchRaysDesc() const;
    [[nodiscard]] ID3D12StateObject* GetPSO() const { return m_pso.Get(); }
    [[nodiscard]] bool IsLastCompileSuccesful() const;
    [[nodiscard]] std::string GetLastCompileError() const;
private:
    void CreateLocalRootSignature(ID3D12Device10* device);
    void CreatePSO(ID3D12Device10* device);
    void CreateShaderTables(ID3D12Device10* device, const std::vector<HitGroupRecord>& hitGroupRecords);

    Microsoft::WRL::ComPtr<ID3D12StateObject> m_pso;
    ID3D12RootSignature* m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_localRootSignature;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_raygenTable;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_missTable;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_hitGroupTable;

    UINT m_raygenRecordSize = 0;
    UINT m_missRecordSize = 0;
    UINT m_hitGroupRecordSize = 0;
    UINT m_hitGroupCount = 0;

    std::unique_ptr<Shader> m_shader;
};