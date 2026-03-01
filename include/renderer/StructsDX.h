#pragma once
#include <d3d12.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <ImReflect.hpp>

struct CameraData
{
	glm::vec3 position;
	float fov;
	glm::vec3 forward;
	float jitterX;
	glm::vec3 right;
	float jitterY;
	glm::vec3 up;
	float aperture;
	float focusDistance;
	uint32_t _pad0;
	uint32_t _pad1;
	uint32_t _pad2;
};
IMGUI_REFLECT(CameraData, fov, aperture, focusDistance, position, forward, right, up)

struct RenderData
{
	CameraData camera = {};
	CameraData prevCamera = {};
	int32_t hdriIndex = -1;
	uint32_t frame = 0;
};

enum DebugMode
{
	None,
	Albedo,
	Emissive,
	Metallic,
	Roughness,
	NormalMap,
	Normal,
	GeoNormal,
	Tangent,
	Bitangent,
	TangentW,
};

enum DLSSQuality
{
	UltraPerformance = 0,
	Performance = 1,
	Balanced = 2,
	Quality = 3,
	DLAA = 4,
};

struct RenderSettings
{
	DebugMode debugMode = None;
	uint32_t bounces = 2;
	float skyIntensity = 1.0f;
	float lightIntensity = 1.0f;
	BOOL whiteAlbedo = false;
	BOOL whiteLighting = false;
	BOOL denoising = false;
	DLSSQuality dlssQuality = Balanced;
};
IMGUI_REFLECT(RenderSettings, debugMode, bounces, skyIntensity, lightIntensity, whiteAlbedo, whiteLighting, denoising, dlssQuality)

enum TonemapOperator
{
	Linear,
	Aces,
	Reinhard,
	AgX,
	GT7,
};

struct PostProcessSettings
{
	TonemapOperator tonemapper = AgX;
	float exposure = 10.0f;
};
IMGUI_REFLECT(PostProcessSettings, tonemapper, exposure)

struct HitGroupRecord
{
	D3D12_GPU_VIRTUAL_ADDRESS vertexBuffer;
	D3D12_GPU_VIRTUAL_ADDRESS indexBuffer;
	uint32_t materialIndex;
	uint32_t isAlphaTested;
};

struct MaterialData
{
	glm::vec4 albedoFactor = glm::vec4(1.0f);
	int32_t albedoIndex = -1;
	glm::vec3 emissiveFactor = glm::vec3(1.0f);
	int32_t emissiveIndex = -1;
	float metallicFactor = 1.0f;
	float roughnessFactor = 1.0f;
	int32_t metallicRoughnessIndex = -1;
	int32_t normalIndex = -1;
	uint32_t isAlphaTested;
	uint32_t _pad1;
};