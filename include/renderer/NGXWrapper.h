#pragma once
#include "StructsDX.h"

#include <d3d12.h>
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_defs.h>
#include <nvsdk_ngx_params.h>
#include <glm/vec2.hpp>
#include <string>

class Window;
struct RenderContext;

class NGXWrapper
{
public:
	NGXWrapper(RenderContext& context, Window& window);
	~NGXWrapper();

	struct DLSSInputs
	{
		ID3D12Resource* albedo;
		ID3D12Resource* specularAlbedo;
		ID3D12Resource* normalRoughness;
		ID3D12Resource* depth;
		ID3D12Resource* motionVectors;
		ID3D12Resource* input;
		ID3D12Resource* output;
		float jitterX;
		float jitterY;
	};

	void Initialize();
	void EvaluateDLSS(ID3D12GraphicsCommandList4* commandList, const DLSSInputs& inputs) const;
	glm::ivec2 Resize();

	void SetDLSSQuality(DLSSQuality qualityMode);

	[[nodiscard]] bool IsDLSSSupported() const { return m_dlssSupported == 1; }
	[[nodiscard]] DLSSQuality GetDLSSQuality() const;
	[[nodiscard]] uint32_t GetRenderWidth() const { return m_renderWidth; }
	[[nodiscard]] uint32_t GetRenderHeight() const { return m_renderHeight; }
	[[nodiscard]] static glm::vec2 GetJitter(int frame);
private:
	void CreateDLSSFeature();
	glm::ivec2 SetOptimalInputRes();

	RenderContext& m_context;
	Window& m_window;
	std::string m_projectId = "309e35e04cf24dc78438741ce522d663";

	NVSDK_NGX_Parameter* m_ngxParameters = nullptr;
	NVSDK_NGX_Handle* m_dlssFeature = nullptr;
	uint32_t m_renderWidth = 0;
	uint32_t m_renderHeight = 0;
	int m_dlssSupported = -1;
	NVSDK_NGX_PerfQuality_Value m_dlssQuality = NVSDK_NGX_PerfQuality_Value_Balanced;
};

