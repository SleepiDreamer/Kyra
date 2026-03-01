#include "RTPipeline.h"
#include "ShaderCompiler.h"
#include "Shader.h"
#include "CommandQueue.h"
#include "StructsDX.h"
#include "CommonDX.h"

#include <d3dx12.h>
#include <iostream>

using namespace Microsoft::WRL;

RTPipeline::RTPipeline(ID3D12Device10* device, ID3D12RootSignature* rootSignature, ShaderCompiler& compiler, const std::vector<HitGroupRecord>& records, const std::string& shaderPath)
	: m_rootSignature(rootSignature)
{
	std::vector<std::string> entryPoints = {};
    m_shader = std::make_unique<Shader>(compiler, shaderPath, entryPoints, true);
    if (!m_shader->IsValid())
    {
        ThrowError("Failed to compile RT shaders: " + shaderPath);
    }

    CreateLocalRootSignature(device);
    CreatePSO(device);
    CreateShaderTables(device, records);
}

RTPipeline::~RTPipeline() = default;

bool RTPipeline::IsLastCompileSuccesful() const
{
	return !m_shader->LastCompileFailed();
}

std::string RTPipeline::GetLastCompileError() const
{
	return m_shader->GetLastCompileError();
}

void RTPipeline::CreateLocalRootSignature(ID3D12Device10* device)
{
    CD3DX12_ROOT_PARAMETER1 params[3] = {};
    params[0].InitAsShaderResourceView(0, 1); // t0:1 vertices
    params[1].InitAsShaderResourceView(1, 1); // t1:1 indices
	params[2].InitAsConstants(2, 0, 1);       // b0:1 material index

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
    desc.Init_1_1(3, params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

    ComPtr<ID3DBlob> sigBlob, errorBlob;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &sigBlob, &errorBlob));
    ThrowIfFailed(device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_localRootSignature)));
	m_localRootSignature->SetName(L"RT Local Root Signature");
}

void RTPipeline::CreatePSO(ID3D12Device10* device)
{
    CD3DX12_STATE_OBJECT_DESC psoDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    auto lib = psoDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    auto bytecode = m_shader->GetBytecode();
    lib->SetDXILLibrary(&bytecode);

    // Hit group
    auto hitGroup = psoDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroup->SetClosestHitShaderImport(L"ClosestHit");
    hitGroup->SetHitGroupExport(L"HitGroup");
    hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

    auto hitGroupAlpha = psoDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroupAlpha->SetClosestHitShaderImport(L"ClosestHit");
    hitGroupAlpha->SetAnyHitShaderImport(L"AnyHit");
    hitGroupAlpha->SetHitGroupExport(L"HitGroupAlpha");
    hitGroupAlpha->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

    auto shaderConfig = psoDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    shaderConfig->Config(
        sizeof(float) * 18, // max payload size
        sizeof(float) * 2  // max attribute size
    );

    auto globalRootSig = psoDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSig->SetRootSignature(m_rootSignature);

    auto localRootSig = psoDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    localRootSig->SetRootSignature(m_localRootSignature.Get());

    auto association = psoDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    association->SetSubobjectToAssociate(*localRootSig);
    association->AddExport(L"HitGroup");
    association->AddExport(L"HitGroupAlpha");

    auto pipelineConfig = psoDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pipelineConfig->Config(2); // max recursion depth

    ThrowIfFailed(device->CreateStateObject(psoDesc, IID_PPV_ARGS(&m_pso)));
	m_pso->SetName(L"RT PSO");
}

void RTPipeline::CreateShaderTables(ID3D12Device10* device, const std::vector<HitGroupRecord>& hitGroupRecords)
{
    ComPtr<ID3D12StateObjectProperties> props;
    ThrowIfFailed(m_pso.As(&props));

    constexpr UINT shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    auto Align = [](UINT size, UINT alignment) -> UINT
    {
        return (size + alignment - 1) & ~(alignment - 1);
    };

    m_raygenRecordSize = Align(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    m_missRecordSize = Align(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    m_hitGroupRecordSize = Align(shaderIdSize + sizeof(HitGroupRecord), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    m_hitGroupCount = static_cast<UINT>(hitGroupRecords.size());

    // Raygen table
    {
        UINT tableSize = Align(m_raygenRecordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(tableSize);
        ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
					  &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_raygenTable)));
        m_raygenTable->SetName(L"Raygen Table");

        void* mapped = nullptr;
        m_raygenTable->Map(0, nullptr, &mapped);
        memcpy(mapped, props->GetShaderIdentifier(L"RayGen"), shaderIdSize);
        m_raygenTable->Unmap(0, nullptr);
    }

    // Miss table
    {
        UINT tableSize = Align(m_missRecordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(tableSize);
        ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
					  &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_missTable)));
		m_missTable->SetName(L"Miss Table");

        void* mapped = nullptr;
        m_missTable->Map(0, nullptr, &mapped);
        memcpy(mapped, props->GetShaderIdentifier(L"Miss"), shaderIdSize);
        m_missTable->Unmap(0, nullptr);
    }

    // Hit group table
    {
        UINT tableSize = Align(m_hitGroupRecordSize * m_hitGroupCount, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufDesc = BUFFER_RESOURCE;
		bufDesc.Width = std::max(1u, m_hitGroupRecordSize * m_hitGroupCount);
        ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
					  &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_hitGroupTable)));
		m_hitGroupTable->SetName(L"Hit Group Table");

        void* mapped = nullptr;
        m_hitGroupTable->Map(0, nullptr, &mapped);
        auto* dst = static_cast<uint8_t*>(mapped);
        auto* shaderId = props->GetShaderIdentifier(L"HitGroup");
        auto* shaderAlphaId = props->GetShaderIdentifier(L"HitGroupAlpha");

        for (UINT i = 0; i < m_hitGroupCount; i++)
        {
			bool isAlphaTested = hitGroupRecords[i].isAlphaTested == 1;
			auto* shader = isAlphaTested ? shaderAlphaId : shaderId;

            auto* record = dst + i * m_hitGroupRecordSize;
            memcpy(record, shader, shaderIdSize);
            memcpy(record + shaderIdSize, &hitGroupRecords[i], sizeof(HitGroupRecord));
        }

        m_hitGroupTable->Unmap(0, nullptr);
    }
}

void RTPipeline::Rebuild(ID3D12Device10* device, const std::vector<HitGroupRecord>& records)
{
    m_pso.Reset();
    m_raygenTable.Reset();
    m_missTable.Reset();
    m_hitGroupTable.Reset();

    CreatePSO(device);
    CreateShaderTables(device, records);
}

void RTPipeline::RebuildShaderTables(ID3D12Device10* device, const std::vector<HitGroupRecord>& records)
{
    m_raygenTable.Reset();
    m_missTable.Reset();
    m_hitGroupTable.Reset();
	CreateShaderTables(device, records);
}

bool RTPipeline::CheckHotReload(ID3D12Device10* device, CommandQueue& commandQueue, const std::vector<HitGroupRecord>& records)
{
    if (!m_shader->NeedsReload())
    {
	    return false;
    }

    if (!m_shader->Reload())
    {
        std::cerr << "[RTPipeline] Hot reload failed, keeping previous shaders\n";
        return false;
    }

    commandQueue.Flush();
    Rebuild(device, records);
    std::cout << "[RTPipeline] Hot reload successful\n";

    return true;
}

D3D12_DISPATCH_RAYS_DESC RTPipeline::GetDispatchRaysDesc() const
{
    D3D12_DISPATCH_RAYS_DESC desc{};

    desc.RayGenerationShaderRecord.StartAddress = m_raygenTable->GetGPUVirtualAddress();
    desc.RayGenerationShaderRecord.SizeInBytes = m_raygenRecordSize;

    desc.MissShaderTable.StartAddress = m_missTable->GetGPUVirtualAddress();
    desc.MissShaderTable.SizeInBytes = m_missRecordSize;
    desc.MissShaderTable.StrideInBytes = m_missRecordSize;

    desc.HitGroupTable.StartAddress = m_hitGroupTable->GetGPUVirtualAddress();
    desc.HitGroupTable.SizeInBytes = m_hitGroupRecordSize * m_hitGroupCount;
    desc.HitGroupTable.StrideInBytes = m_hitGroupRecordSize;

    desc.Width = 0;
    desc.Height = 0;
    desc.Depth = 1;

    return desc;
}