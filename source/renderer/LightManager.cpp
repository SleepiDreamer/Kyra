#include "LightManager.h"
#include "CommandQueue.h"
#include "ComputePass.h"
#include "GPUAllocator.h"
#include "GPUBuffer.h"
#include "StructuredBuffer.h"
#include "Model.h"

#include <pix3.h>
#include <DirectXMath.h>

struct ReadbackCopy
{
	StructuredBuffer* src;
	ID3D12Resource*   dst;
	UINT64            bytes;
};

struct Counters
{
	uint32_t numLights;
	float totalPower;
	uint32_t numLightEntries;
	uint32_t numHeavyEntries;
};

void CopyBuffersToReadback(ID3D12GraphicsCommandList4* cmd, const std::vector<ReadbackCopy>& copies)
{
	if (copies.empty()) return;

	for (const auto& c : copies)
		c.src->Transition(cmd, D3D12_RESOURCE_STATE_COPY_SOURCE);

	for (const auto& c : copies)
		cmd->CopyBufferRegion(c.dst, 0, c.src->GetResource(), 0, c.bytes);

	for (const auto& c : copies)
		c.src->Transition(cmd, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

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

	m_counters = std::make_unique<StructuredBuffer>(
		m_context, 1, sizeof(uint32_t) * 4,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Emissive Counters Buffer");

	m_countersReadback = m_context.allocator->CreateBuffer(
		sizeof(uint32_t) * 4, D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Emissive Counter Readback");

	auto sampler = POINT_SAMPLER;
	m_parseLightsPass = std::make_unique<ComputePass>(m_context, "shaders/emissive/emissive_parse.slang", "ParseLights", "ParseEmissives", sampler);
	m_parsePass = std::make_unique<ComputePass>(m_context, "shaders/emissive/emissive_parse.slang", "ParseTriangles", "ParseEmissives", sampler);
	m_partitionPass = std::make_unique<ComputePass>(m_context, "shaders/emissive/partition_pass.slang", "Partition", "AliasPartition");
	m_scanLocalPass = std::make_unique<ComputePass>(m_context, "shaders/emissive/prefixsum_pass.slang", "ScanLocal", "AliasScanLocal");
	m_scanBlockSumsPass = std::make_unique<ComputePass>(m_context,"shaders/emissive/prefixsum_pass.slang", "ScanBlockSums", "AliasScanBlockSums");
	m_scanAddOffsetsPass = std::make_unique<ComputePass>(m_context, "shaders/emissive/prefixsum_pass.slang", "ScanAddOffsets", "AliasScanAddOffsets");
	m_splitPass = std::make_unique<ComputePass>(m_context, "shaders/emissive/split_pass.slang", "Split", "AliasSplit");
	m_packPass = std::make_unique<ComputePass>(m_context, "shaders/emissive/pack_pass.slang", "Pack", "AliasPack");

	m_lightsReadback = m_context.allocator->CreateBuffer(
		numLights * sizeof(Light), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Emissive Lights Readback");
	m_powerReadback = m_context.allocator->CreateBuffer(
		numLights * sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Emissive Power Readback");
	m_lightPrefixReadback = m_context.allocator->CreateBuffer(
		numLights * sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Emissive Light Prefix Readback");
	m_heavyPrefixReadback = m_context.allocator->CreateBuffer(
		numLights * sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Emissive Heavy Prefix Readback");
	m_lightReadback = m_context.allocator->CreateBuffer(
		numLights * sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Emissive Light Readback");
	m_heavyReadback = m_context.allocator->CreateBuffer(
		numLights * sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Emissive Heavy Readback");
	m_tableReadback = m_context.allocator->CreateBuffer(
		numLights * sizeof(uint32_t) * 2, D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Emissive Alias Table Readback");
	m_blocksumsReadback = m_context.allocator->CreateBuffer(
		numBlocks * sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Emissive Block Sums Readback");
	m_splitsSpillBufferReadback = m_context.allocator->CreateBuffer(
		splitsCapacity * sizeof(uint32_t) * 2, D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Emissive Split/Spill Readback");
}

void LightManager::AddLights(const std::vector<Light>& lights)
{
	m_pending = true;

	for (const auto& light : lights)
	{
		m_pendingLights.push_back(light);
	}
}

static uint32_t floatToUint(const float f)
{
	uint32_t u;
	std::memcpy(&u, &f, sizeof(u));
	return u;
};

void LightManager::CopyLightsToCPU(ID3D12GraphicsCommandList4* commandList)
{
	m_lightsReadback.Transition(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	m_lightBuffer->Transition(commandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->CopyBufferRegion(m_lightsReadback.resource, 0, m_lightBuffer->GetResource(), 0, m_lightBuffer->GetSize());
}

void LightManager::UploadPendingLights(ID3D12GraphicsCommandList4* commandList)
{
	if (!m_pending || m_pendingLights.empty()) return;

	std::vector<Light> lights;
	uint32_t numNewLights = static_cast<uint32_t>(m_pendingLights.size());

	// Map light readback
	{
		Light* mapped = nullptr;
		D3D12_RANGE readRange = { 0, m_lightBuffer->GetSize() };
		m_lightsReadback.resource->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
		lights = std::vector<Light>(mapped, mapped + m_numLights);
		D3D12_RANGE writeRange = { 0, 0 };
		m_lightsReadback.resource->Unmap(0, &writeRange);
	}
	
	// Insert pending lights
	{
		lights.insert(lights.end(), m_pendingLights.begin(), m_pendingLights.end());
		m_pendingLights.clear();
		m_pending = false;
	}

	// Copy lights to GPU buffer
	{
		m_lightBuffer->Update(lights.data(), static_cast<uint32_t>(lights.size()));
	}

	// Parse new lights
	{
		m_lightBuffer->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_counters->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_powerBuffer->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		ComputePass::ComputeBindings bindings;
		bindings.srvCount = 0;
		bindings.uavs[0] = m_lightBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[1] = m_counters->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[2] = m_powerBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.uavCount = 3;
		bindings.rootConstants[0] = floatToUint(numNewLights);
		bindings.rootConstants[1] = floatToUint(m_numLights);
		bindings.rootConstantCount = 2;
		bindings.threads.x = numNewLights;
		m_parseLightsPass->Dispatch(commandList, bindings);

		m_counters->UAVBarrier(commandList);
	}
}

void LightManager::LoadEmissiveVertices(Model& model, const StructuredBuffer* materialBuffer, ID3D12GraphicsCommandList4* commandList) const
{
	PIXScopedEvent(commandList, 0x76b900, "Load Emissive Vertices");

	m_lightBuffer->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_counters->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_powerBuffer->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

	m_counters->UAVBarrier(commandList);

	m_context.commandQueue->Flush();

	m_counters->Transition(commandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->CopyBufferRegion(m_countersReadback.resource, 0, m_counters->GetResource(), 0, m_counters->GetSize());
	m_counters->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void LightManager::NumLightsCallback()
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
}

void LightManager::BuildAliasTable(ID3D12GraphicsCommandList4* commandList)
{
	PIXScopedEvent(commandList, 0x76b900, "Build Alias Table");

	Log::Info("LightManager: {} lights, total power: {}", m_numLights, m_totalPower);

	// Partition pass
	{
		m_powerBuffer->Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_counters->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_aliasLight->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_aliasHeavy->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		ComputePass::ComputeBindings bindings;
		bindings.srvs[0] = m_powerBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.srvCount = 1;
		bindings.uavs[0] = m_counters->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[1] = m_aliasLight->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[2] = m_aliasHeavy->GetResource()->GetGPUVirtualAddress();
		bindings.uavCount = 3;
		bindings.rootConstantCount = 0;
		bindings.threads.x = m_numLights;
		bindings.threadGroupSize.x = 32;
		m_partitionPass->Dispatch(commandList, bindings);

		m_counters->UAVBarrier(commandList);
	}

	constexpr uint32_t kScanGroup = 256;
	const uint32_t numBlocks = (m_numLights + kScanGroup - 1) / kScanGroup;

	// Prefix sum pass
	{
		auto runScan = [&](StructuredBuffer* list, StructuredBuffer* prefixOut, const uint32_t heavyFlag)
		{

			// per-block local scan + block total
			{
				list->Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				prefixOut->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				m_blockSums->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				m_counters->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

				prefixOut->UAVBarrier(commandList);
				m_blockSums->UAVBarrier(commandList);
				m_counters->UAVBarrier(commandList);
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

				m_blockSums->UAVBarrier(commandList);
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

				prefixOut->UAVBarrier(commandList);
				m_blockSums->UAVBarrier(commandList);
			}
			};

		runScan(m_aliasLight.get(), m_lightPrefix.get(), 0);
		runScan(m_aliasHeavy.get(), m_heavyPrefix.get(), 1);
	}

	const uint32_t s = std::min(splitsCapacity - 1, m_numLights);

	// Split pass
	{
		m_lightPrefix->Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_heavyPrefix->Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_splitsSpillBuffer->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_counters->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		ComputePass::ComputeBindings bindings;
		bindings.srvs[0] = m_lightPrefix->GetResource()->GetGPUVirtualAddress();
		bindings.srvs[1] = m_heavyPrefix->GetResource()->GetGPUVirtualAddress();
		bindings.srvCount = 2;
		bindings.uavs[0] = m_splitsSpillBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[1] = m_counters->GetResource()->GetGPUVirtualAddress();
		bindings.uavCount = 2;
		bindings.rootConstants[0] = s;
		bindings.rootConstants[1] = m_numLights;
		bindings.rootConstantCount = 2;
		bindings.threads.x = s + 1;
		bindings.threadGroupSize.x = 64;
		m_splitPass->Dispatch(commandList, bindings);

		m_splitsSpillBuffer->UAVBarrier(commandList);
		m_counters->UAVBarrier(commandList);
	}

	// Pack pass
	if (true)
	{
		m_aliasLight->Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_aliasHeavy->Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_splitsSpillBuffer->Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_aliasTable->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_counters->Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		ComputePass::ComputeBindings bindings;
		bindings.srvs[0] = m_powerBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.srvs[1] = m_aliasLight->GetResource()->GetGPUVirtualAddress();  // t1: light worklist
		bindings.srvs[2] = m_aliasHeavy->GetResource()->GetGPUVirtualAddress();  // t2: heavy worklist
		bindings.srvs[3] = m_splitsSpillBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.srvCount = 4;
		bindings.uavs[0] = m_aliasTable->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[1] = m_counters->GetResource()->GetGPUVirtualAddress();
		bindings.uavCount = 2;
		bindings.rootConstants[0] = s;
		bindings.rootConstants[1] = m_numLights;
		bindings.rootConstantCount = 2;
		bindings.threads.x = s;
		bindings.threadGroupSize.x = 64;
		m_packPass->Dispatch(commandList, bindings);

		m_aliasTable->UAVBarrier(commandList);
		m_counters->UAVBarrier(commandList);
	}

	std::vector<ReadbackCopy> copies = {
		{ m_lightBuffer.get(),       m_lightsReadback.resource,            m_numLights * sizeof(Light)          },
		{ m_powerBuffer.get(),       m_powerReadback.resource,             m_numLights * sizeof(uint32_t)       },
		{ m_aliasLight.get(),        m_lightReadback.resource,             m_numLights * sizeof(uint32_t)       },
		{ m_aliasHeavy.get(),        m_heavyReadback.resource,             m_numLights * sizeof(uint32_t)       },
		{ m_lightPrefix.get(),       m_lightPrefixReadback.resource,       m_numLights * sizeof(uint32_t)       },
		{ m_heavyPrefix.get(),       m_heavyPrefixReadback.resource,       m_numLights * sizeof(uint32_t)       },
		{ m_blockSums.get(),         m_blocksumsReadback.resource,         numBlocks * sizeof(uint32_t)         },
		{ m_splitsSpillBuffer.get(), m_splitsSpillBufferReadback.resource, splitsCapacity * sizeof(uint32_t) * 2  },
		{ m_aliasTable.get(),        m_tableReadback.resource,             m_numLights * sizeof(uint32_t) * 2   },
		{ m_counters.get(),          m_countersReadback.resource,          sizeof(uint32_t) * 4                 } };
	CopyBuffersToReadback(commandList, copies);
}

void LightManager::UpdateAliasCounters(const uint32_t numLights, const float totalPower) const
{
	struct Counters
	{
		uint32_t numLights;
		float totalPower;
		uint32_t numLightEntries;
		uint32_t numHeavyEntries;
	};
	Counters counters = { numLights, totalPower, 0, 0 };
	m_counters->Update(&counters, 0);
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

namespace {
	struct AliasRowRB { float    threshold; uint32_t alias; }; // table stride = 8
	struct SplitRB { uint32_t j;          float    spill; }; // splits stride = 8

	template <typename T>
	std::vector<T> Readback(ID3D12Resource* res, uint32_t count)
	{
		std::vector<T> out(count);
		if (count == 0) return out;
		void* mapped = nullptr;
		D3D12_RANGE readRange = { 0, count * sizeof(T) };
		res->Map(0, &readRange, &mapped);
		std::memcpy(out.data(), mapped, count * sizeof(T));
		D3D12_RANGE writeRange = { 0, 0 };
		res->Unmap(0, &writeRange);
		return out;
	}
}

void LightManager::DumpReadbacks() const
{
	const uint32_t N = m_numLights;                          // active count (26), not capacity

	// Counters first, so we know how far the worklists are actually filled.
	uint32_t nL = N, nH = 0;
	float    W = m_totalPower;
	{
		auto c = Readback<uint32_t>(m_countersReadback.resource, 4); // {numLights, totalPower(float bits), nL, nH}
		std::memcpy(&W, &c[1], sizeof(float));
		nL = c[2];
		nH = c[3];
	}
	const uint32_t s = std::min(splitsCapacity - 1, N);
	const uint32_t numBlocks = (N + 255) / 256;
	const float    WN = W / (float)N;

	printf("\n========== EMISSIVE READBACK DUMP (N=%u, W=%.6f, W/N=%.6f, nL=%u, nH=%u) ==========\n",
		N, W, WN, nL, nH);
	if (nL + nH != N)
		printf("  !! WARNING: nL + nH (%u) != N (%u) -- partition dropped items.\n", nL + nH, N);

	// ---- powers (float bits in a uint-stride buffer) ----
	{
		auto p = Readback<float>(m_powerReadback.resource, N);
		printf("\n-- powers --\n");
		for (uint32_t i = 0; i < N; i++) printf("  [%3u] %.6f\n", i, p[i]);
	}

	// ---- light / heavy worklists (uint indices; only nL / nH are valid) ----
	{
		auto l = Readback<uint32_t>(m_lightReadback.resource, nL);
		auto h = Readback<uint32_t>(m_heavyReadback.resource, nH);
		printf("\n-- light worklist (%u) --\n  ", nL);
		for (uint32_t i = 0; i < nL; i++) printf("%u ", l[i]);
		printf("\n-- heavy worklist (%u) --\n  ", nH);
		for (uint32_t i = 0; i < nH; i++) printf("%u ", h[i]);
		printf("\n");

		// sanity: do the worklists cover every index 0..N-1 exactly once?
		std::vector<int> seen(N, 0);
		bool bad = false;
		for (uint32_t i = 0; i < nL; i++) { if (l[i] >= N || seen[l[i]]++) bad = true; }
		for (uint32_t i = 0; i < nH; i++) { if (h[i] >= N || seen[h[i]]++) bad = true; }
		for (uint32_t i = 0; i < N; i++) if (seen[i] != 1) bad = true;
		printf("  worklist coverage: %s\n", bad ? "BAD (missing/dup/oob index)" : "ok (clean permutation)");
	}

	// ---- prefix sums (float bits) ----
	{
		auto lp = Readback<float>(m_lightPrefixReadback.resource, nL);
		auto hp = Readback<float>(m_heavyPrefixReadback.resource, nH);
		printf("\n-- light prefix (inclusive weights) --\n  ");
		for (uint32_t i = 0; i < nL; i++) printf("%.4f ", lp[i]);
		printf("\n-- heavy prefix (inclusive weights) --\n  ");
		for (uint32_t i = 0; i < nH; i++) printf("%.4f ", hp[i]);
		printf("\n");
	}

	// ---- block sums (float bits) ----
	{
		auto b = Readback<float>(m_blocksumsReadback.resource, numBlocks);
		printf("\n-- block sums (%u) --\n  ", numBlocks);
		for (uint32_t i = 0; i < numBlocks; i++) printf("%.4f ", b[i]);
		printf("\n");
	}

	// ---- splits (j is uint, spill is float) : boundaries 0..s ----
	{
		auto sp = Readback<SplitRB>(m_splitsSpillBufferReadback.resource, s + 1);
		printf("\n-- splits (boundaries 0..%u) --\n", s);
		for (uint32_t k = 0; k <= s; k++) printf("  [%3u] j=%u spill=%.6f\n", k, sp[k].j, sp[k].spill);
	}

	// ---- alias table (threshold is float, alias is uint) ----
	{
		auto t = Readback<AliasRowRB>(m_tableReadback.resource, N);
		printf("\n-- alias table --\n");
		for (uint32_t i = 0; i < N; i++)
			printf("  [%3u] threshold=%.6f  alias=%u  (selfP=%.4f)\n",
				i, t[i].threshold, t[i].alias,
				WN > 0.0f ? t[i].threshold / WN : 0.0f);
	}

	printf("================================================================================\n\n");
}