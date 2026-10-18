#pragma once
#include "CommonDX.h"
#include "StructsDX.h"

class PostProcessPass;
class RenderTexture;
class Texture;
struct PostProcessSettings;
template<typename T>
class CBVBuffer;

class Tonemapper
{
public:
	Tonemapper(RenderContext& context, const std::shared_ptr<CBVBuffer<PostProcessSettings>>& settings);
	~Tonemapper();
	
	[[nodiscard]] const std::vector<std::string>& GetTonemapNamesSDR() const { return m_tonemapOperatorsSdr; }
	[[nodiscard]] const std::vector<std::string>& GetTonemapNamesHDR() const { return m_tonemapOperatorsHdr; }
	[[nodiscard]] uint32_t GetActiveTonemapSDR() const { return m_activeTonemapSdr; }
	[[nodiscard]] uint32_t GetActiveTonemapHDR() const { return m_activeTonemapHdr; }
	
	void SetActiveTonemapSDR(const std::string& name);
	void SetActiveTonemapHDR(const std::string& name);
	
	void Tonemap(ID3D12GraphicsCommandList4* commandList, RenderTexture& input, 
		RenderTexture& output, uint32_t backBufferIndex, bool hdrEnabled) const;
	
private:
	void LoadLUT(const std::string& path, const std::string& name);
	
	RenderContext& m_context;
	std::unique_ptr<PostProcessPass> m_tonemappingPass;
	std::shared_ptr<CBVBuffer<PostProcessSettings>> m_settings;
	std::unordered_map<std::string, Texture> m_luts;
	
	std::vector<std::string> m_tonemapOperatorsSdr = 
		{ "Linear", "OpenDRT SDR", "OpenDRT SDR (Colorful)", "AgX", "ACES", "Reinhard", "GT7" };
	std::vector<std::string> m_tonemapOperatorsHdr = 
		{ "Linear", "OpenDRT HDR", "OpenDRT HDR (Colorful)", "Psycho", "Custom" };
	
	uint32_t m_activeTonemapSdr = 2; // Default: OpenDRT-Colorful
	uint32_t m_activeTonemapHdr = 2; // Default: OpenDRT-Colorful
};