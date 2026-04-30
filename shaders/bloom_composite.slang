// Based on Alexander Christensen's LearnOpenGL guest article on bloom: https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

import settings;
import structs;

Texture2D<float4> inputTexture : register(t0, space0);
RWTexture2D<float4> outputTexture : register(u0, space0);
ConstantBuffer<RenderSettings> renderSettings : register(b0, space0);
ConstantBuffer<RenderData> renderData : register(b1, space0);
ConstantBuffer<PostProcessSettings> settings : register(b2, space0);
ConstantBuffer<MipData> mipData : register(b4, space0);
SamplerState bloomSampler : register(s0, space0);

[shader("compute")]
[numthreads(8, 8, 1)]
void BloomComposite(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= mipData.dstSize.x || dtid.y >= mipData.dstSize.y) return;
    float2 uv = (float2(dtid.xy) + 0.5) / float2(mipData.dstSize);
    float x = bloomFilterRadius;
    float y = bloomFilterRadius * mipData.dstSize.y / mipData.dstSize.x;

    float3 a = inputTexture.SampleLevel(bloomSampler, float2(uv.x - x, uv.y + y), 0).rgb;
    float3 b = inputTexture.SampleLevel(bloomSampler, float2(uv.x, uv.y + y), 0).rgb;
    float3 c = inputTexture.SampleLevel(bloomSampler, float2(uv.x + x, uv.y + y), 0).rgb;

    float3 d = inputTexture.SampleLevel(bloomSampler, float2(uv.x - x, uv.y), 0).rgb;
    float3 e = inputTexture.SampleLevel(bloomSampler, float2(uv.x, uv.y), 0).rgb;
    float3 f = inputTexture.SampleLevel(bloomSampler, float2(uv.x + x, uv.y), 0).rgb;

    float3 g = inputTexture.SampleLevel(bloomSampler, float2(uv.x - x, uv.y - y), 0).rgb;
    float3 h = inputTexture.SampleLevel(bloomSampler, float2(uv.x, uv.y - y), 0).rgb;
    float3 i = inputTexture.SampleLevel(bloomSampler, float2(uv.x + x, uv.y - y), 0).rgb;

    float3 upsample = e * 4.0;
    upsample += (b + d + f + h) * 2.0;
    upsample += (a + c + g + i);
    upsample *= 1.0 / 16.0;

    float3 result = lerp(outputTexture[dtid.xy].rgb, upsample / mipData.totalMips, settings.bloomStrength);

    outputTexture[dtid.xy] = float4(result, 1.0);
}