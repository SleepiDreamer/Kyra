#pragma once
#include "Light.h"
#include "CommonDX.h"
#include "GPUBuffer.h"

class Model;
class ComputePass;
class StructuredBuffer;

class LightManager
{
public:
	LightManager(RenderContext& context);
	~LightManager() = default;

	void AddLights(const std::vector<Light>& lights);
	void CopyLightsToCPU(ID3D12GraphicsCommandList4* commandList);
	void UploadPendingLights(ID3D12GraphicsCommandList4* commandList);
	void LoadEmissiveVertices(Model& model, const StructuredBuffer* materialBuffer, ID3D12GraphicsCommandList4* commandList) const;
	void NumLightsCallback();
	void BuildAliasTable(ID3D12GraphicsCommandList4* commandList);
	void UpdateAliasCounters(uint32_t numLights, float totalPower) const;

	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetLightBufferAddress() const;
	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetPowerBufferAddress() const;
	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetAliasTableAddress() const;
	[[nodiscard]] uint32_t GetNumLights() const { return m_numLights; }
	[[nodiscard]] float GetTotalPower() const { return m_totalPower; }

	void DumpReadbacks() const;

private:
	RenderContext& m_context;

	std::unique_ptr<StructuredBuffer> m_lightBuffer;
	std::vector<Light> m_pendingLights;
	bool m_pending = false;
	uint32_t m_numLights = 0;
	float m_totalPower = 0.0f;

	std::unique_ptr<StructuredBuffer> m_counters;
	std::unique_ptr<StructuredBuffer> m_powerBuffer;
	std::unique_ptr<StructuredBuffer> m_aliasTable;
	std::unique_ptr<StructuredBuffer> m_aliasLight;
	std::unique_ptr<StructuredBuffer> m_aliasHeavy;
	std::unique_ptr<StructuredBuffer> m_lightPrefix;
	std::unique_ptr<StructuredBuffer> m_heavyPrefix;
	std::unique_ptr<StructuredBuffer> m_blockSums;
	std::unique_ptr<StructuredBuffer> m_splitsSpillBuffer;
	GPUBuffer m_countersReadback;

	std::unique_ptr<ComputePass> m_parseLightsPass;
	std::unique_ptr<ComputePass> m_parsePass;
	std::unique_ptr<ComputePass> m_partitionPass;

	std::unique_ptr<ComputePass> m_scanLocalPass;
	std::unique_ptr<ComputePass> m_scanBlockSumsPass;
	std::unique_ptr<ComputePass> m_scanAddOffsetsPass;

	static constexpr uint32_t splitsCapacity = 1024;
	std::unique_ptr<ComputePass> m_splitPass;
	std::unique_ptr<ComputePass> m_packPass;

	GPUBuffer m_lightsReadback;
	GPUBuffer m_powerReadback;
	GPUBuffer m_lightPrefixReadback;
	GPUBuffer m_heavyPrefixReadback;
	GPUBuffer m_lightReadback;
	GPUBuffer m_heavyReadback;
	GPUBuffer m_tableReadback;
	GPUBuffer m_blocksumsReadback;
	GPUBuffer m_splitsSpillBufferReadback;
};