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
#include "StructsDX.h"
#include "Log.h"

#include <stb_image.h>

Scene::Scene(RenderContext& context)
	: m_context(context)
{
	m_lightBuffer = std::make_unique<StructuredBuffer>(m_context, 256, sizeof(Light), D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_DEFAULT, "Light Buffer");
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

	m_context.commandQueue->ExecuteCommandList(commandList);
	m_context.commandQueue->Flush();

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
		m_lightBuffer->Update(m_lights.data(), (m_lights.size() + 255) & ~255);
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