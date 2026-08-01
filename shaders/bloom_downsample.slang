// Based on Alexander Christensen's LearnOpenGL guest article on bloom: https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

import structs;
import helpers;

Texture2D<float4> inputTexture : register(t0, space0);
RWTexture2D<float4> outputTexture : register(u0, space0);
ConstantBuffer<RenderSettings> renderSettings : register(b0, space0);
ConstantBuffer<RenderData> renderData : register(b1, space0);
ConstantBuffer<PostProcessSettings> settings : register(b2, space0);
ConstantBuffer<MipData> mipData : register(b4, space0);
SamplerState bloomSampler : register(s0, space0);

float KarisAverage(float3 color)
{
    float luma = Grayscale(LinearToSrgb(color)) * 0.25;
    return 1.0 / (1.0 + luma);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void BloomDownsample(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= mipData.dstSize.x || dtid.y >= mipData.dstSize.y) return;
    float2 uv = (float2(dtid.xy) + 0.5) / float2(mipData.dstSize);
    float2 srcTexelSize = 1.0 / float2(mipData.srcSize);

    float3 a = inputTexture.SampleLevel(bloomSampler, float2(uv.x - 2 * srcTexelSize.x, uv.y + 2 * srcTexelSize.y), 0).rgb;
    float3 b = inputTexture.SampleLevel(bloomSampler, float2(uv.x, uv.y + 2 * srcTexelSize.y), 0).rgb;
    float3 c = inputTexture.SampleLevel(bloomSampler, float2(uv.x + 2 * srcTexelSize.x, uv.y + 2 * srcTexelSize.y), 0).rgb;

    float3 d = inputTexture.SampleLevel(bloomSampler, float2(uv.x - 2 * srcTexelSize.x, uv.y), 0).rgb;
    float3 e = inputTexture.SampleLevel(bloomSampler, float2(uv.x, uv.y), 0).rgb;
    float3 f = inputTexture.SampleLevel(bloomSampler, float2(uv.x + 2 * srcTexelSize.x, uv.y), 0).rgb;

    float3 g = inputTexture.SampleLevel(bloomSampler, float2(uv.x - 2 * srcTexelSize.x, uv.y - 2 * srcTexelSize.y), 0).rgb;
    float3 h = inputTexture.SampleLevel(bloomSampler, float2(uv.x, uv.y - 2 * srcTexelSize.y), 0).rgb;
    float3 i = inputTexture.SampleLevel(bloomSampler, float2(uv.x + 2 * srcTexelSize.x, uv.y - 2 * srcTexelSize.y), 0).rgb;

    float3 j = inputTexture.SampleLevel(bloomSampler, float2(uv.x - srcTexelSize.x, uv.y + srcTexelSize.y), 0).rgb;
    float3 k = inputTexture.SampleLevel(bloomSampler, float2(uv.x + srcTexelSize.x, uv.y + srcTexelSize.y), 0).rgb;
    float3 l = inputTexture.SampleLevel(bloomSampler, float2(uv.x - srcTexelSize.x, uv.y - srcTexelSize.y), 0).rgb;
    float3 m = inputTexture.SampleLevel(bloomSampler, float2(uv.x + srcTexelSize.x, uv.y - srcTexelSize.y), 0).rgb;

    float3 downsample = float3(0.0, 0.0, 0.0);
    if (mipData.mipLevel == 0)
    {
        float3 groups[5];
        groups[0] = (a + b + d + e) * (0.125f / 4.0f);
        groups[1] = (b + c + e + f) * (0.125f / 4.0f);
        groups[2] = (d + e + g + h) * (0.125f / 4.0f);
        groups[3] = (e + f + h + i) * (0.125f / 4.0f);
        groups[4] = (j + k + l + m) * (0.5f / 4.0f);
        groups[0] *= KarisAverage(groups[0]);
        groups[1] *= KarisAverage(groups[1]);
        groups[2] *= KarisAverage(groups[2]);
        groups[3] *= KarisAverage(groups[3]);
        groups[4] *= KarisAverage(groups[4]);
        downsample = groups[0] + groups[1] + groups[2] + groups[3] + groups[4];

        downsample = max(downsample, 0.0000001);
    }
    else
    {
        downsample = e * 0.125;
        downsample += (a + c + g + i) * 0.03125;
        downsample += (b + d + f + h) * 0.0625;
        downsample += (j + k + l + m) * 0.125;
    }

    outputTexture[dtid.xy] = float4(downsample, 1.0);
}