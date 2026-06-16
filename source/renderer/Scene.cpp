#include "Scene.h"
#include "Model.h"
#include "Mesh.h"
#include "TLAS.h"
#include "Light.h"
#include "Texture.h"
#include "CommandQueue.h"
#include "UploadContext.h"
#include "GPUAllocator.h"
#include "StructuredBuffer.h"
#include "ComputePass.h"
#include "StructsDX.h"
#include "Log.h"

#include <stb_image.h>

#include "PostProcessPass.h"

Scene::Scene(RenderContext& context)
	: m_context(context)
{
	constexpr uint32_t numLights = 1024;
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

Scene::~Scene() = default;

bool Scene::LoadModel(const std::string& path)
{
	std::string extension = path.substr(path.find_last_of('.'));
	if (extension != ".gltf" && extension != ".glb")
	{
		Log::Error("Unsupported model format: {}. Please use .gltf or .glb instead", extension);
		return false;
	}

	Log::Info("Loading model: {}", path);
	std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

	auto commandList = m_context.commandQueue->GetCommandList();
	m_models.emplace_back(m_context, commandList.Get(), path);

	m_context.uploadContext->Flush();

	m_context.commandQueue->ExecuteCommandList(commandList);
	m_context.commandQueue->Flush();

	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances = {};
	int instanceId = 0;
	for (const auto& model : m_models)
	{
		for (const auto& mesh : model.GetMeshes())
		{
			instances.emplace_back(mesh.GetInstanceDesc(instanceId++));
		}
	}
	m_tlas = std::make_unique<TLAS>(m_context);
	m_tlas->Build(m_context.device, instances);

	UploadMaterialData();

	m_context.uploadContext->Flush();
	m_context.commandQueue->Flush();

	commandList = m_context.commandQueue->GetCommandList();

	auto& newModel = m_models.back();
	for (auto& tex : newModel.GetTextures())
	{
		if (tex.GetResource())
		{
			TransitionResource(commandList.Get(), tex.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}
	AddLights(newModel.GetLights());

	LoadEmissiveVertices(newModel, commandList.Get());

	m_context.commandQueue->ExecuteCommandList(commandList);
	m_context.commandQueue->Flush();

	// Emissive light counter readback
	{
		uint32_t* mapped = nullptr;
		D3D12_RANGE readRange = { 0, sizeof(uint32_t) };
		m_emissiveCounterReadback.resource->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
		m_numLights = mapped[0];
		D3D12_RANGE writeRange = { 0, 0 };
		m_emissiveCounterReadback.resource->Unmap(0, &writeRange);
	}

	auto time = std::chrono::steady_clock::now() - startTime;
	Log::Success("Loaded model: {}. Took {:.2f} s.", path, std::chrono::duration_cast<std::chrono::milliseconds>(time).count() / 1000.0);

	return true;
}

void Scene::LoadHDRI(const std::string& path)
{
	m_context.commandQueue->Flush();

	if (m_hdri)
	{
		m_hdri.reset();
	}

	std::string extension = path.substr(path.find_last_of('.'));
	if (extension != ".hdr")
	{
		Log::Error("Unsupported HDRI format: {}. Please use .hdr instead", extension);
		return;
	}
	
	m_hdri = std::make_unique<Texture>();

	Log::Info("Loading HDRI: {}", path);
	std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
	int width, height, nrChannels;
	float* data = stbi_loadf(path.c_str(), &width, &height, &nrChannels, 4);
	if (data)
	{
		m_hdri->Create(m_context, data, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, path);
		stbi_image_free(data);

		m_context.uploadContext->Flush();

		auto commandList = m_context.commandQueue->GetCommandList();
		TransitionResource(commandList.Get(), m_hdri->GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_context.commandQueue->ExecuteCommandList(commandList);
		m_context.commandQueue->Flush();
	}
	else
	{
		Log::Error("Failed to load HDRI: {}", path);
	}
	auto time = std::chrono::steady_clock::now() - startTime;
	Log::Success("Loaded HDRI: {}. Took {:.2f} s.", path, std::chrono::duration_cast<std::chrono::milliseconds>(time).count() / 1000.0);
}

void Scene::AddLights(const std::vector<Light>& lights)
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

void Scene::UploadMaterialData()
{
	std::vector<MaterialData> materials;
	for (const auto& model : m_models)
	{
		for (const auto& texture : model.GetMaterials())
		{
			materials.push_back({
				texture.albedoFactor,
				texture.albedoIndex,
				texture.emissiveFactor,
				texture.emissiveIndex,
				texture.metallicFactor,
				texture.roughnessFactor,
				texture.metallicRoughnessIndex,
				texture.normalIndex,
				texture.samplerIndex,
				texture.ior,
				texture.flags,
				texture.uvTransform
			});
		}
	}

	uint32_t numMaterials = static_cast<uint32_t>(materials.size());

	m_materialBuffer = std::make_unique<StructuredBuffer>(
		m_context, numMaterials, sizeof(MaterialData),
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_DEFAULT, "Materials");

	m_context.uploadContext->Upload(
		m_materialBuffer->GetBuffer(), materials.data(), numMaterials * sizeof(MaterialData));
}

static uint32_t floatToUint(const float f)
{
	uint32_t u; 
	std::memcpy(&u, &f, sizeof(u)); 
	return u;
};

void Scene::LoadEmissiveVertices(const Model& model, ID3D12GraphicsCommandList4* commandList) const
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
		bindings.srvs[2] = m_materialBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.srvCount = 3;
		bindings.uavs[0] = m_lightBuffer->GetResource()->GetGPUVirtualAddress();
		bindings.uavs[1] = m_emissiveCounter->GetResource()->GetGPUVirtualAddress();
		bindings.uavCount = 2;
		bindings.rootConstants[0] =  floatToUint(transform._11);
		bindings.rootConstants[1] =  floatToUint(transform._21);
		bindings.rootConstants[2] =  floatToUint(transform._31);
		bindings.rootConstants[3] =  floatToUint(transform._41);
		bindings.rootConstants[4] =  floatToUint(transform._12);
		bindings.rootConstants[5] =  floatToUint(transform._22);
		bindings.rootConstants[6] =  floatToUint(transform._32);
		bindings.rootConstants[7] = floatToUint(transform._42);
		bindings.rootConstants[8] = floatToUint(transform._13);
		bindings.rootConstants[9] = floatToUint(transform._23);
		bindings.rootConstants[10] = floatToUint(transform._33);
		bindings.rootConstants[11] = floatToUint(transform._43);
		bindings.rootConstants[12] = numTriangles;
		bindings.rootConstants[13] = mesh.m_materialIndex;
		bindings.rootConstants[14] = mesh.GetIndexSRV().index;
		bindings.rootConstants[15] = mesh.GetVertexSRV().index;
		bindings.rootConstantCount = 16;
		bindings.threads = numTriangles;
		m_emissiveComputePass->Dispatch(commandList, bindings);
	}

	D3D12_RESOURCE_BARRIER uavBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::UAV(m_emissiveCounter->GetResource())
	};
	commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);

	m_context.commandQueue->Flush();

	// after all dispatches:
	CD3DX12_RESOURCE_BARRIER toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
		m_emissiveCounter->GetResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &toCopy);

	commandList->CopyBufferRegion(m_emissiveCounterReadback.resource, 0,
		m_emissiveCounter->GetResource(), 0, sizeof(uint32_t));

	// transition back if the buffer is touched again later this submit
	CD3DX12_RESOURCE_BARRIER back = CD3DX12_RESOURCE_BARRIER::Transition(
		m_emissiveCounter->GetResource(),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &back);
}

D3D12_GPU_VIRTUAL_ADDRESS Scene::GetTLASAddress() const
{
	return m_tlas ? m_tlas->GetResource().resource->GetGPUVirtualAddress() : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS Scene::GetMaterialsBufferAddress() const
{
	return m_materialBuffer ? m_materialBuffer->GetResource() ? m_materialBuffer->GetResource()->GetGPUVirtualAddress() : 0 : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS Scene::GetLightBufferAddress() const
{
	return m_lightBuffer ? m_lightBuffer->GetResource() ? m_lightBuffer->GetResource()->GetGPUVirtualAddress() : 0 : 0;
}

std::vector<HitGroupRecord> Scene::GetHitGroupRecords() const
{
	std::vector<HitGroupRecord> records;
	uint32_t materialOffset = 0;
	for (const auto& model : m_models)
	{
		for (const auto& mesh : model.GetMeshes())
		{
			HitGroupRecord record{};
			record.vertexBuffer = mesh.GetVertexBuffer()->GetGPUVirtualAddress();
			record.indexBuffer = mesh.GetIndexBuffer()->GetGPUVirtualAddress();
			record.materialIndex = mesh.m_materialIndex >= 0
				? materialOffset + static_cast<uint32_t>(mesh.m_materialIndex)
				: 0;
			if (mesh.m_materialIndex >= 0 && mesh.m_materialIndex < model.GetMaterials().size())
			{
				record.isAlphaTested = model.GetMaterials()[mesh.m_materialIndex].flags & MAT_FLAG_TRANSPARENT ? 1 : 0;
			}
			records.push_back(record);
		}
		materialOffset += static_cast<uint32_t>(model.GetMaterials().size());
	}
	return records;
}

int32_t Scene::GetHDRIDescriptorIndex() const
{
	return m_hdri ? m_hdri->GetDescriptorIndex() : -1;
}