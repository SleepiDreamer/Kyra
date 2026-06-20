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
	void LoadEmissiveVertices(Model& model, const StructuredBuffer* materialBuffer, ID3D12GraphicsCommandList4* commandList) const;
	void ReadCounterCallback();

	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetLightBufferAddress() const;
	[[nodiscard]] uint32_t GetNumLights() const { return m_numLights; }

private:
	RenderContext& m_context;

	std::unique_ptr<StructuredBuffer> m_lightBuffer;
	std::vector<Light> m_lights;
	uint32_t m_numLights = 0;

	std::unique_ptr<ComputePass> m_emissiveComputePass;
	std::unique_ptr<StructuredBuffer> m_emissiveCounter;
	GPUBuffer m_emissiveCounterReadback;
};