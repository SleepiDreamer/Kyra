#include "LightManager.h"
#include "CommandQueue.h"
#include "ComputePass.h"
#include "GPUAllocator.h"
#include "GPUBuffer.h"
#include "StructuredBuffer.h"

#include <DirectXMath.h>

#include "Model.h"


LightManager::LightManager(RenderContext& context)
	: m_context(context)
{
	constexpr uint32_t numLights = 1048576; // 2^20
	m_lightBuffer = std::make_unique<StructuredBuffer>(
		m_context, numLights, static_cast<uint32_t>(sizeof(Light)),
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Light Buffer");

	m_powerBuffer = std::make_unique<StructuredBuffer>(
		m_context, numLights, static_cast<uint32_t>(sizeof(uint32_t)),
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Emissive Power Buffer");

	m_aliasTable = std::make_unique<StructuredBuffer>(
		m_context, numLights, static_cast<uint32_t>(sizeof(uint32_t) * 2),
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Emissive Alias Table Buffer");

	m_aliasLight = std::make_unique<StructuredBuffer>(
		m_context, numLights, static_cast<uint32_t>(sizeof(uint32_t)),
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Emissive Alias (Light) Buffer");

	m_aliasHeavy = std::make_unique<StructuredBuffer>(
		m_context, numLights, static_cast<uint32_t>(sizeof(uint32_t)),
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Emissive Alias (Heavy) Buffer");


	m_lightPrefix = std::make_unique<StructuredBuffer>(
		m_context, numLights, static_cast<uint32_t>(sizeof(uint32_t)),
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Emissive Prefix Sum Scratch Buffer");

	m_heavyPrefix = std::make_unique<StructuredBuffer>(
		m_context, numLights, static_cast<uint32_t>(sizeof(uint32_t)),
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Emissive Heavy Prefix Buffer");

	constexpr uint32_t scanGroup = 256;
	constexpr uint32_t numBlocks = numLights / scanGroup;
	m_blockSums = std::make_unique<StructuredBuffer>(
		m_context, numBlocks, static_cast<uint32_t>(sizeof(uint32_t)),
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Emissive Scan Block Sums Buffer");

	m_splitsSpillBuffer = std::make_unique<StructuredBuffer>(
		m_context, splitsCapacity, static_cast<uint32_t>(sizeof(uint32_t) * 2),
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Emissive Split/Spill Buffer");

	// numLights, total power
	m_counters = std::make_unique<StructuredBuffer>(
		m_context, 1, sizeof(uint32_t) * 4,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Emissive Counters Buffer");

	m_countersReadback = m_context.allocator->CreateBuffer(
		sizeof(uint32_t) * 4, D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Emissive Counter Readback");

	auto sampler = POINT_SAMPLER;
	m_parsePass = std::make_unique<ComputePass>(m_context, "shaders/emissive/emissive_parse.slang", "ParseEmissives", "ParseEmissives", sampler);
	m_partitionPass = std::make_unique<ComputePass>(m_context, "shaders/emissive/partition_pass.slang", "Partition", "AliasPartition");
	m_scanLocalPass = std::make_unique<ComputePass>(m_context, "shaders/emissive/prefixsum_pass.slang", "ScanLocal", "AliasScanLocal");
	m_scanBlockSumsPass = std::make_unique<ComputePass>(m_context,"shaders/emissive/prefixsum_pass.slang", "ScanBlockSums", "AliasScanBlockSums");
	m_scanAddOffsetsPass = std::make_unique<ComputePass>(m_context, "shaders/emissive/prefixsum_pass.slang", "ScanAddOffsets", "AliasScanAddOffsets");
	m_splitPass = std::make_unique<ComputePass>(m_context, "shaders/emissive/split_pass.slang", "Split", "AliasSplit");
	m_packPass = std::make_unique<ComputePass>(m_context, "shaders/emissive/pack_pass.slang", "Pack", "AliasPack");
}

void LightManager::AddLights(const std::vector<Light>& lights)
{
	for (const auto& light : lights)
	{
		m_lights.push_back(light);
	}
	if (!m_lights.empty())
	{
		m_lightBuffer->Update(m_lights.data(), static_cast<uint32_t>(m_lights.size()));
	}
}

static uint32_t floatToUint(const float f)
{
	uint32_t u;
	std::memcpy(&u, &f, sizeof(u));
	return u;
};

void LightManager::LoadEmissiveVertices(Model& model, const StructuredBuffer* materialBuffer, ID3D12GraphicsCommandList4* commandList) const
{
	auto& meshes = model.GetMeshes();
	for (auto& mesh : meshes)
	{
		if (mesh.m_materialIndex < 0) continue;

		uint32_t numIndices = mesh.GetIndexCount();
		uint32_t numTriangles = numIndices / 3;

		DirectX::XMFLOAT4X4 transform = mesh.GetTransform();

		ComputePass::ComputeBindings bindings;
		bindings.srvs[0] = mesh.GetVertexBuffer()->GetGPUVirtualAddress();
		bindings.srvs[1] = mesh.GetIndexBuffer()->GetGPUVirtualAddress();
		bindings.srvs[2] = materialBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.srvCount = 3;
		bindings.uavs[0] = m_lightBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[1] = m_counters->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[2] = m_powerBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.uavCount = 3;
		bindings.rootConstants[0] = floatToUint(transform._11);
		bindings.rootConstants[1] = floatToUint(transform._21);
		bindings.rootConstants[2] = floatToUint(transform._31);
		bindings.rootConstants[3] = floatToUint(transform._41);
		bindings.rootConstants[4] = floatToUint(transform._12);
		bindings.rootConstants[5] = floatToUint(transform._22);
		bindings.rootConstants[6] = floatToUint(transform._32);
		bindings.rootConstants[7] = floatToUint(transform._42);
		bindings.rootConstants[8] = floatToUint(transform._13);
		bindings.rootConstants[9] = floatToUint(transform._23);
		bindings.rootConstants[10] = floatToUint(transform._33);
		bindings.rootConstants[11] = floatToUint(transform._43);
		bindings.rootConstants[12] = numTriangles;
		bindings.rootConstants[13] = mesh.m_materialIndex;
		bindings.rootConstantCount = 14;
		bindings.threads.x = numTriangles;
		m_parsePass->Dispatch(commandList, bindings);
	}

	D3D12_RESOURCE_BARRIER uavBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::UAV(m_counters->GetResource())
	};
	commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);

	m_context.commandQueue->Flush();

	CD3DX12_RESOURCE_BARRIER toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
		m_counters->GetResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &toCopy);

	commandList->CopyBufferRegion(m_countersReadback.resource, 0,
		m_counters->GetResource(), 0, m_counters->GetSize());

	CD3DX12_RESOURCE_BARRIER back = CD3DX12_RESOURCE_BARRIER::Transition(
		m_counters->GetResource(),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &back);
}

void LightManager::BuildAliasTable(ID3D12GraphicsCommandList4* commandList)
{
	// Readbacks
	{
		// uint
		{
			uint32_t* mapped = nullptr;
			D3D12_RANGE readRange = { 0, m_counters->GetSize() };
			m_countersReadback.resource->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
			m_numLights = mapped[0];
			D3D12_RANGE writeRange = { 0, 0 };
			m_countersReadback.resource->Unmap(0, &writeRange);
		}

		// float
		{
			float* mapped = nullptr;
			D3D12_RANGE readRange = { 0, m_counters->GetSize() };
			m_countersReadback.resource->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
			m_totalPower = mapped[1];
			D3D12_RANGE writeRange = { 0, 0 };
			m_countersReadback.resource->Unmap(0, &writeRange);
		}
		Log::Info("LightManager: {} emissive lights, total power: {}", m_numLights, m_totalPower);
	}

	// Partition pass
	{
		ComputePass::ComputeBindings bindings;
		bindings.srvs[0] = m_powerBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.srvCount = 1;
		bindings.uavs[0] = m_counters->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[1] = m_aliasLight->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[2] = m_aliasHeavy->GetResource()->GetGPUVirtualAddress();
		bindings.uavCount = 3;
		bindings.rootConstants[0];
		bindings.rootConstantCount = 0;
		bindings.threads.x = m_numLights;
		bindings.threadGroupSize.x = 32;
		m_partitionPass->Dispatch(commandList, bindings);

		D3D12_RESOURCE_BARRIER afterPartition[] = {
			CD3DX12_RESOURCE_BARRIER::UAV(m_counters->GetResource()),
			CD3DX12_RESOURCE_BARRIER::UAV(m_aliasLight->GetResource()),
			CD3DX12_RESOURCE_BARRIER::UAV(m_aliasHeavy->GetResource()),
		};
		commandList->ResourceBarrier(_countof(afterPartition), afterPartition);
	}

	// Prefix sum pass
	{
		auto runScan = [&](const StructuredBuffer* list, const StructuredBuffer* prefixOut, const uint32_t heavyFlag)
		{
			constexpr uint32_t kScanGroup = 256;
			const uint32_t numBlocks = (m_numLights + kScanGroup - 1) / kScanGroup;

			// per-block local scan + block total
			{
				ComputePass::ComputeBindings bindings;
				bindings.srvs[0] = m_powerBuffer->GetResource()->GetGPUVirtualAddress();
				bindings.srvs[1] = list->GetResource()->GetGPUVirtualAddress();
				bindings.srvCount = 2;
				bindings.uavs[0] = prefixOut->GetResource()->GetGPUVirtualAddress();
				bindings.uavs[1] = m_blockSums->GetResource()->GetGPUVirtualAddress();
				bindings.uavs[2] = m_counters->GetResource()->GetGPUVirtualAddress();
				bindings.uavCount = 3;
				bindings.rootConstants[0] = heavyFlag;
				bindings.rootConstants[1] = numBlocks;
				bindings.rootConstantCount = 2;
				bindings.threads.x = m_numLights;
				bindings.threadGroupSize.x = 256;
				m_scanLocalPass->Dispatch(commandList, bindings);

				D3D12_RESOURCE_BARRIER uavBarriers[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(prefixOut->GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(m_blockSums->GetResource())
				};
				commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);
			}

			// scan block totals
			{
				ComputePass::ComputeBindings bindings;
				bindings.uavs[0] = m_blockSums->GetResource()->GetGPUVirtualAddress();
				bindings.uavCount = 1;
				bindings.rootConstants[0] = heavyFlag;
				bindings.rootConstants[1] = numBlocks;
				bindings.rootConstantCount = 2;
				bindings.threads.x = 1;
				bindings.threadGroupSize.x = 1;
				m_scanBlockSumsPass->Dispatch(commandList, bindings);

				D3D12_RESOURCE_BARRIER uavBarriers[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(m_blockSums->GetResource())
				};
				commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);
			}

			// add offset back
			{
				ComputePass::ComputeBindings bindings;
				bindings.uavs[0] = prefixOut->GetResource()->GetGPUVirtualAddress();
				bindings.uavs[1] = m_blockSums->GetResource()->GetGPUVirtualAddress();
				bindings.uavCount = 2;
				bindings.rootConstants[0] = heavyFlag;
				bindings.rootConstants[1] = numBlocks;
				bindings.rootConstantCount = 2;
				bindings.threads.x = m_numLights;
				bindings.threadGroupSize.x = 256;
				m_scanAddOffsetsPass->Dispatch(commandList, bindings);

				D3D12_RESOURCE_BARRIER uavBarriers[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(prefixOut->GetResource())
				};
				commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);
			}
		};

		runScan(m_aliasLight.get(), m_lightPrefix.get(), 0);
		runScan(m_aliasHeavy.get(), m_heavyPrefix.get(), 1);
	}

	// Split pass
	{
		ComputePass::ComputeBindings bindings;
		bindings.srvs[0] = m_lightPrefix->GetResource()->GetGPUVirtualAddress();
		bindings.srvs[1] = m_heavyPrefix->GetResource()->GetGPUVirtualAddress();
		bindings.srvCount = 2;
		bindings.uavs[0] = m_splitsSpillBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[1] = m_counters->GetResource()->GetGPUVirtualAddress();
		bindings.uavCount = 2;
		bindings.rootConstants[0] = splitsCapacity - 1;
		bindings.rootConstants[1] = m_numLights;
		bindings.rootConstantCount = 2;
		bindings.threads.x = splitsCapacity;
		bindings.threadGroupSize.x = 64;
		m_splitPass->Dispatch(commandList, bindings);

		D3D12_RESOURCE_BARRIER afterSplit[] = {
			CD3DX12_RESOURCE_BARRIER::UAV(m_splitsSpillBuffer->GetResource()),
		};
		commandList->ResourceBarrier(_countof(afterSplit), afterSplit);
	}

	// Pack pass
	{
		ComputePass::ComputeBindings bindings;
		bindings.srvs[0] = m_powerBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.srvs[1] = m_lightPrefix->GetResource()->GetGPUVirtualAddress();
		bindings.srvs[2] = m_heavyPrefix->GetResource()->GetGPUVirtualAddress();
		bindings.srvs[3] = m_splitsSpillBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.srvCount = 4;
		bindings.uavs[0] = m_aliasTable->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[1] = m_counters->GetResource()->GetGPUVirtualAddress();
		bindings.uavCount = 2;
		bindings.rootConstants[0] = splitsCapacity - 1;
		bindings.rootConstants[1] = m_numLights;
		bindings.rootConstantCount = 2;
		bindings.threads.x = splitsCapacity - 1;
		bindings.threadGroupSize.x = 64;
		m_packPass->Dispatch(commandList, bindings);
	}
}

D3D12_GPU_VIRTUAL_ADDRESS LightManager::GetLightBufferAddress() const
{
	return m_lightBuffer ? m_lightBuffer->GetResource() ? m_lightBuffer->GetResource()->GetGPUVirtualAddress() : 0 : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS LightManager::GetPowerBufferAddress() const
{
	return m_powerBuffer ? m_powerBuffer->GetResource() ? m_powerBuffer->GetResource()->GetGPUVirtualAddress() : 0 : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS LightManager::GetAliasTableAddress() const
{
	return m_aliasTable ? m_aliasTable->GetResource() ? m_aliasTable->GetResource()->GetGPUVirtualAddress() : 0 : 0;
}