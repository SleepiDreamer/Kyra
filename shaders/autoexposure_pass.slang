import structs;
import helpers;
import settings;

Texture2D<float4> inputTexture : register(t0, space0);
RWBuffer<float> outputUav : register(u0, space0);
ConstantBuffer<RenderSettings> renderSettings : register(b0, space0);
ConstantBuffer<RenderData> renderData : register(b1, space0);
ConstantBuffer<PostProcessSettings> settings : register(b2, space0);
SamplerState linearSampler : register(s0, space0);

static const int AE_SAMPLES2 = AE_SAMPLES * AE_SAMPLES;

[shader("compute")]
[numthreads(1, 1, 1)]
void AutoExposure(uint3 dtid : SV_DispatchThreadID)
{
    uint2 resolution;
    inputTexture.GetDimensions(resolution.x, resolution.y);

    float totalLogLum = 0;
    for (int j = 0; j < AE_SAMPLES; j++) {
        for (int i = 0; i < AE_SAMPLES; i++) {
            float2 uv = float2((i + 0.5) / AE_SAMPLES, (j + 0.5) / AE_SAMPLES);
            float3 c = sanitize(inputTexture.SampleLevel(linearSampler, uv, 0).rgb);
            totalLogLum += log(max(Grayscale(c), 0.001));
        }
    }
    
    float avgLogLum = totalLogLum / AE_SAMPLES2;
    float avgLuminance = exp(avgLogLum);

    float prevExposure = outputUav[0];
    float targetExposure = clamp(settings.targetExposure / avgLuminance, MIN_EXPOSURE, MAX_EXPOSURE);
    float adaptationSpeed = targetExposure > prevExposure ? ADAPTATION_SPEED_UP : ADAPTATION_SPEED_DOWN;
    float deltaTime = min(renderData.deltaTime, 0.1f);
    float newExposure = lerp(prevExposure, targetExposure, 1.0 - exp(-adaptationSpeed * deltaTime));
    newExposure = clamp(newExposure, prevExposure * 0.8f, prevExposure * 1.25f);

    // Immediately set exposure if previous was black
    if (prevExposure < 0.0001)
    {
        outputUav[0] = targetExposure;
        return;
    }

    outputUav[0] = newExposure;
}