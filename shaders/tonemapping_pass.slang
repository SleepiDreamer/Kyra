import structs;
import helpers;
import tonemapping;

Texture2D<float4> inputTexture : register(t0, space0);
RWTexture2D<float4> outputTexture : register(u0, space0);
ConstantBuffer<RenderSettings> renderSettings : register(b0, space0);
ConstantBuffer<RenderData> renderData : register(b1, space0);
ConstantBuffer<PostProcessSettings> settings : register(b2, space0);
SamplerState linearSampler : register(s0, space0);

[shader("compute")]
[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint2 resolution;
    outputTexture.GetDimensions(resolution.x, resolution.y);
    if (dtid.x >= resolution.x || dtid.y >= resolution.y) return;

    float3 result;
    if (renderSettings.upscaling)
    {
        result = inputTexture[dtid.xy].rgb;
    }
    else
    {
        float3 accumulated = inputTexture[dtid.xy].rgb;
        result = accumulated / (renderData.frame + 1);
    }

    if (renderSettings.debugMode == DebugMode::None)
    { 
        result = tonemap(result, settings.tonemapper, settings.exposure);
    }

    outputTexture[dtid.xy] = float4(result, 1.0);
}