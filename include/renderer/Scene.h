#pragma once
#include "Light.h"
#include "LightManager.h"

class Model;
class TLAS;
class Texture;
class CommandQueue;
class GPUAllocator;
class StructuredBuffer;
class ComputePass;
class PostProcessPass;
class LightManager;
struct HitGroupRecord;

class Scene
{
public:
	Scene(RenderContext& context);
	~Scene();

	bool LoadModel(const std::string& path);
	void LoadHDRI(const std::string& path);
	void AddLights(const std::vector<Light>& lights) const;

	[[nodiscard]] const std::vector<Model>& GetModels() const { return m_models; }
	[[nodiscard]] const TLAS& GetTLAS() const { return *m_tlas; }
	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetTLASAddress() const;
	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetMaterialsBufferAddress() const;
	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetLightBufferAddress() const { return m_lightManager->GetLightBufferAddress(); }
	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetPowerBufferAddress() const { return m_lightManager->GetPowerBufferAddress(); }
	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetLightAliasTableBufferAddress() const { return m_lightManager->GetAliasTableAddress(); }
	[[nodiscard]] std::vector<HitGroupRecord> GetHitGroupRecords();
	[[nodiscard]] int32_t GetHDRIDescriptorIndex() const;
	[[nodiscard]] uint32_t GetNumLights() const { return m_lightManager->GetNumLights(); }
	[[nodiscard]] float GetTotalLightPower() const { return m_lightManager->GetTotalPower(); }

private:
	void UploadMaterialData();

	RenderContext& m_context;

	std::unique_ptr<TLAS> m_tlas;
	std::vector<Model> m_models;
	std::unique_ptr<Texture> m_hdri;
	std::unique_ptr<StructuredBuffer> m_materialBuffer;
	uint32_t m_materialIdxOffset = 1; // default material

	std::unique_ptr<LightManager> m_lightManager;
};