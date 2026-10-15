import structs;
import helpers;
import tonemapping.aces;
import tonemapping.reinhard;
import tonemapping.agx;
import tonemapping.gt7;
import tonemapping.hdr;
import tonemapping.psycho;

Texture2D<float4> inputTexture : register(t0, space0);
RWTexture2D<float4> outputTexture : register(u0, space0);
ConstantBuffer<PostProcessSettings> settings : register(b0, space0);
SamplerState linearSampler : register(s0, space0);

public float3 TonemapSDR(float3 luminance, TonemapOperatorSDR op, float exposure)
{
    luminance *= exposure;
    luminance = ACEScgToRec709(luminance);

    switch (op)
    {
    default: return luminance;
    case TonemapOperatorSDR::Linear: return luminance;
    case TonemapOperatorSDR::Aces: return TonemapACES(luminance);
    case TonemapOperatorSDR::Reinhard: return TonemapReinhard(luminance);
    case TonemapOperatorSDR::AgX: return TonemapAgX(luminance);
    case TonemapOperatorSDR::GT7: return TonemapGT7(luminance);
    }
}

public float3 TonemapHDR(float3 luminance, TonemapOperatorHDR op, float maxNits, float exposure)
{
    luminance *= exposure;
    luminance = ACEScgToRec2020(luminance);

    float3 tonemapped;

    switch (op)
    {
    default: tonemapped = TonemapHDRLinear(luminance, maxNits); break;
    case TonemapOperatorHDR::Linear: tonemapped = TonemapHDRLinear(luminance, maxNits); break;
    case TonemapOperatorHDR::Custom: tonemapped = TonemapHDRCustom(luminance, maxNits); break;
    case TonemapOperatorHDR::Psycho: tonemapped = TonemapPsycho(luminance, maxNits); break;
    }

    return LinearToPQ(tonemapped);
}

public float3 DebugViewToHDR(float3 color, float paperWhite)
{
    float3 linear = SrgbToLinear(color);
    linear = Rec709ToACEScg(linear);
    linear = ACEScgToRec2020(linear);
    float3 PQ = LinearToPQ(linear * paperWhite);
    return PQ;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void Tonemapping(uint3 dtid : SV_DispatchThreadID)
{
    uint2 resolution;
    outputTexture.GetDimensions(resolution.x, resolution.y);
    if (dtid.x >= resolution.x || dtid.y >= resolution.y) return;

    const float peakNits = 800.0;
    const float paperWhite = 200.0;

    float3 result;
    result = inputTexture[dtid.xy].rgb;

        if (settings.hdr)
        {
            if (settings.skipTonemap)
            {
                // Interpret debug views as SDR sRGB instead of HDR ACEScg
                result = DebugViewToHDR(result, paperWhite);
            }
            else
            {
                result = TonemapHDR(result, settings.tonemapperHDR, peakNits, settings.exposure);
            }
        }
        else
        {
            if (!settings.skipTonemap)
            {
                result = TonemapSDR(result, settings.tonemapperSDR, settings.exposure);
            }
        }

    outputTexture[dtid.xy] = float4(result, 1.0);
}

module tonemapping;
