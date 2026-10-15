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

	void Tonemap(ID3D12GraphicsCommandList4* commandList, RenderTexture& input, RenderTexture& output, uint32_t backBufferIndex) const;

private:
	RenderContext& m_context;
	std::unique_ptr<PostProcessPass> m_tonemappingPass;
	std::vector<Texture> m_LUTs;
	std::shared_ptr<CBVBuffer<PostProcessSettings>> m_settings;
};