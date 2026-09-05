module tonemapping;

import structs;
import helpers;
import tonemapping.aces;
import tonemapping.reinhard;
import tonemapping.agx;
import tonemapping.gt7;
import tonemapping.hdr;
import tonemapping.psycho;

public float3 TonemapSDR(float3 luminance, TonemapOperatorSDR op, float exposure)
{
    luminance *= exposure;
    luminance = ACEScgToRec709(luminance);

    switch (op)
    {
    default:                            return luminance;
    case TonemapOperatorSDR::Linear:    return luminance;
    case TonemapOperatorSDR::Aces:      return TonemapACES(luminance);
    case TonemapOperatorSDR::Reinhard:  return TonemapReinhard(luminance);
    case TonemapOperatorSDR::AgX:       return TonemapAgX(luminance);
    case TonemapOperatorSDR::GT7:       return TonemapGT7(luminance);
    }
}

public float3 TonemapHDR(float3 luminance, TonemapOperatorHDR op, float maxNits, float exposure)
{
    luminance *= exposure;
    luminance = ACEScgToRec2020(luminance);

    float3 tonemapped;

    switch (op)
    {
    default:                           tonemapped = TonemapHDRLinear(luminance, maxNits); break;
    case TonemapOperatorHDR::Linear:   tonemapped = TonemapHDRLinear(luminance, maxNits); break;
    case TonemapOperatorHDR::Custom:   tonemapped = TonemapHDRCustom(luminance, maxNits); break;
    case TonemapOperatorHDR::Psycho:   tonemapped = TonemapPsycho(luminance, maxNits); break;
    }

    return LinearToPQ(tonemapped);
}