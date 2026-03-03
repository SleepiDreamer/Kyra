import structs;
import helpers;
import settings;

Texture2D<float4> inputTexture : register(t0, space0);
RWBuffer<float> outputUav : register(u0, space0);
ConstantBuffer<RenderData> renderData : register(b0, space0);
ConstantBuffer<PostProcessSettings> settings : register(b1, space0);
SamplerState linearSampler : register(s0, space0);

static const int AE_SAMPLES2 = AE_SAMPLES * AE_SAMPLES;

[shader("compute")]
[numthreads(1, 1, 1)]
void AutoExposure(uint3 dtid : SV_DispatchThreadID)
{
    uint2 resolution;
    inputTexture.GetDimensions(resolution.x, resolution.y);

    float2 uv = 1.0 / (resolution + 1);

    float totalLogLum = 0;
    for (int j = 0; j < AE_SAMPLES; j++)
    {
        for (int i = 0; i < AE_SAMPLES; i++)
        {
            float3 c = inputTexture.SampleLevel(linearSampler, uv, 0).rgb;
            totalLogLum += log(max(Grayscale(c), 0.001));
            uv.x += 1.0 / AE_SAMPLES;
        }
        uv.x = 1.0 / (resolution.x + 1);
        uv.y += 1.0 / AE_SAMPLES;
    }
    float avgLuminance = exp(totalLogLum / AE_SAMPLES2);

    float exposure = settings.targetExposure / max(avgLuminance, 0.001);
    exposure = clamp(exposure, MIN_EXPOSURE, MAX_EXPOSURE);

    float prevExposure = outputUav[0];
    float targetExposure = settings.targetExposure / max(avgLuminance, 0.001);
    targetExposure = clamp(targetExposure, MIN_EXPOSURE, MAX_EXPOSURE);

    if (prevExposure < 0.001)
    {
        outputUav[0] = targetExposure;
        return;
    }

    float newExposure = prevExposure + (targetExposure - prevExposure) * (1.0 - exp(-renderData.deltaTime * ADAPTATION_SPEED));
    newExposure = clamp(newExposure, MIN_EXPOSURE, MAX_EXPOSURE);
    outputUav[0] = newExposure;
}