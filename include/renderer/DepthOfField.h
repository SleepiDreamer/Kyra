#pragma once
#include "CommonDX.h"
#include "StructsDX.h"

class PostProcessPass;
class RenderTexture;
class Texture;
struct PostProcessSettings;
template<typename T>
class CBVBuffer;

class DepthOfField
{
public:
	DepthOfField(RenderContext& context, uint32_t width, uint32_t height, const std::shared_ptr<CBVBuffer<PostProcessSettings>>& settings);
	~DepthOfField();

	void Resize(uint32_t width, uint32_t height) const;

	struct DepthOfFieldInput
	{
		RenderTexture* color;
		RenderTexture* depth;
	};

	void Render(ID3D12GraphicsCommandList4* commandList, const DepthOfFieldInput&, uint32_t backBufferIndex) const;

private:
	RenderContext& m_context;
	std::shared_ptr<CBVBuffer<PostProcessSettings>> m_settings;

	std::unique_ptr<PostProcessPass> m_fieldPass;
	std::unique_ptr<PostProcessPass> m_downscalePass;
	std::unique_ptr<PostProcessPass> m_combinePass;

	std::unique_ptr<RenderTexture> m_nearFieldTexture;
	std::unique_ptr<RenderTexture> m_farFieldTexture;
	std::unique_ptr<RenderTexture> m_outputTexture;
};