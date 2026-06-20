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

	auto sampler = POINT_SAMPLER;
	m_emissiveComputePass = std::make_unique<ComputePass>(
		m_context, "shaders/emissive_parse.slang", "ParseEmissives", sampler);

	m_emissiveCounter = std::make_unique<StructuredBuffer>(
		m_context, 1, sizeof(uint32_t),
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Light Counter Buffer");

	m_emissiveCounterReadback = m_context.allocator->CreateBuffer(
		sizeof(uint32_t) * 1, D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Emissive Counter Readback");
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
	ID3D12DescriptorHeap* heap = { m_context.descriptorHeap->GetHeap() };
	commandList->SetDescriptorHeaps(1, &heap);

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
		bindings.uavs[1] = m_emissiveCounter->GetResource()->GetGPUVirtualAddress();
		bindings.uavCount = 2;
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
		bindings.threads = numTriangles;
		m_emissiveComputePass->Dispatch(commandList, bindings);
	}

	D3D12_RESOURCE_BARRIER uavBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::UAV(m_emissiveCounter->GetResource())
	};
	commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);

	m_context.commandQueue->Flush();

	CD3DX12_RESOURCE_BARRIER toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
		m_emissiveCounter->GetResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &toCopy);

	commandList->CopyBufferRegion(m_emissiveCounterReadback.resource, 0,
		m_emissiveCounter->GetResource(), 0, sizeof(uint32_t));

	CD3DX12_RESOURCE_BARRIER back = CD3DX12_RESOURCE_BARRIER::Transition(
		m_emissiveCounter->GetResource(),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &back);
}

void LightManager::ReadCounterCallback()
{
	uint32_t* mapped = nullptr;
	D3D12_RANGE readRange = { 0, sizeof(uint32_t) };
	m_emissiveCounterReadback.resource->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
	m_numLights = mapped[0];
	D3D12_RANGE writeRange = { 0, 0 };
	m_emissiveCounterReadback.resource->Unmap(0, &writeRange);
}

D3D12_GPU_VIRTUAL_ADDRESS LightManager::GetLightBufferAddress() const
{
	return m_lightBuffer ? m_lightBuffer->GetResource() ? m_lightBuffer->GetResource()->GetGPUVirtualAddress() : 0 : 0;

}
