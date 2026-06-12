import structs;
import helpers;
import settings;

Texture2D<float4> inputTexture : register(t0, space0);
RWBuffer<float> outputUav : register(u0, space0);
ConstantBuffer<RenderSettings> renderSettings : register(b0, space0);
ConstantBuffer<RenderData> renderData : register(b1, space0);
ConstantBuffer<PostProcessSettings> settings : register(b2, space0);
SamplerState linearSampler : register(s0, space0);

[shader("compute")]
[numthreads(1, 1, 1)]
void AutoFocus(uint3 dtid: SV_DispatchThreadID)
{
    float deltaTime = min(renderData.deltaTime, 0.1f);

    float currentFocus = clamp(outputUav[1], 0.01, 1024);
    float perfectFocus = clamp(inputTexture.SampleLevel(linearSampler, float2(0.5, 0.5), 0).r, 0.3, 1024);

    float currentDiopter = 1.0 / currentFocus;
    float targetDiopter = 1.0 / perfectFocus;

    float newD = lerp(currentDiopter, targetDiopter, 1.0 - exp(-AF_SPEED * deltaTime));

    outputUav[1] = 1.0 / newD;
}