#pragma once
#include "GPUBuffer.h"
#include "DescriptorHeap.h"

class Model;
class TLAS;
class Texture;
class CommandQueue;
class GPUAllocator;
class StructuredBuffer;
struct HitGroupRecord;

class Scene
{
public:
	Scene(RenderContext& context);
	~Scene();

	bool LoadModel(const std::string& path);
	void LoadHDRI(const std::string& path);

	[[nodiscard]] const std::vector<Model>& GetModels() const { return m_models; }
	[[nodiscard]] const TLAS& GetTLAS() const { return *m_tlas; }
	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetTLASAddress() const;
	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetMaterialsBufferAddress() const;
	[[nodiscard]] std::vector<HitGroupRecord> GetHitGroupRecords() const;
	[[nodiscard]] int32_t GetHDRIDescriptorIndex() const;

private:
	void UploadMaterialData();

	RenderContext& m_context;
	std::unique_ptr<TLAS> m_tlas;
	std::vector<Model> m_models;
	std::unique_ptr<Texture> m_hdri;
	std::unique_ptr<StructuredBuffer> m_materialBuffer;
};

