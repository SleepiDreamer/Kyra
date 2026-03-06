#include "Scene.h"
#include "Model.h"
#include "Mesh.h"
#include "TLAS.h"
#include "Texture.h"
#include "CommandQueue.h"
#include "UploadContext.h"
#include "GPUAllocator.h"
#include "StructuredBuffer.h"
#include "StructsDX.h"

#include <stb_image.h>

Scene::Scene(RenderContext& context)
	: m_context(context)
{
}

Scene::~Scene() = default;

bool Scene::LoadModel(const std::string& path)
{
	std::string extension = path.substr(path.find_last_of('.'));
	if (extension != ".gltf" && extension != ".glb")
	{
		std::cerr << "Unsupported model format: " << extension << "\nPlease use .gltf or .glb\n";
		return false;
	}

	std::cout << "Loading model: " << path;
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

	m_context.commandQueue->ExecuteCommandList(commandList);
	m_context.commandQueue->Flush();

	auto time = std::chrono::steady_clock::now() - startTime;
	std::cout << "\r";
	std::cout << "Loaded model: " << path << ". Took " << std::chrono::duration_cast<std::chrono::milliseconds>(time).count() / 1000.0 << " s.\n";

	return true;
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
				texture.samplerIndex
			});
		}
	}

	m_materialBuffer = std::make_unique<StructuredBuffer>(
		m_context, static_cast<uint32_t>(materials.size()), sizeof(MaterialData),
		D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_DEFAULT, "Materials");

	m_context.uploadContext->Upload(
		m_materialBuffer->GetBuffer(), materials.data(), materials.size() * sizeof(MaterialData));
}

D3D12_GPU_VIRTUAL_ADDRESS Scene::GetTLASAddress() const
{
	return m_tlas ? m_tlas->GetResource().resource->GetGPUVirtualAddress() : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS Scene::GetMaterialsBufferAddress() const
{
	return m_materialBuffer ? m_materialBuffer->GetResource() ? m_materialBuffer->GetResource()->GetGPUVirtualAddress() : 0 : 0;
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
				record.isAlphaTested = model.GetMaterials()[mesh.m_materialIndex].isAlphaTested;
			}
			records.push_back(record);
		}
		materialOffset += static_cast<uint32_t>(model.GetMaterials().size());
	}
	return records;
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
		std::cerr << "Unsupported HDRI format: " << extension << "\nPlease use .hdr\n";
		return;
	}
	
	m_hdri = std::make_unique<Texture>();

	std::cout << "Loading HDRI: " << path << "\r";
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
		ThrowError("Failed to load HDRI: " + path);
	}
	auto time = std::chrono::steady_clock::now() - startTime;
	std::cout << "Loaded HDRI: " << path << ". Took " << std::chrono::duration_cast<std::chrono::milliseconds>(time).count() / 1000.0 << " s.\n";
}

int32_t Scene::GetHDRIDescriptorIndex() const
{
	return m_hdri ? m_hdri->GetDescriptorIndex() : -1;
}