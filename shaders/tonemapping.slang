module tonemapping;

import structs;
import tonemapping.aces;
import tonemapping.reinhard;
import tonemapping.agx;
import tonemapping.gt7;
import tonemapping.hdr;
import tonemapping.psycho;

public float3 TonemapSDR(float3 color, TonemapOperatorSDR op, float exposure)
{
    color *= exposure;

    switch (op)
    {
        default:                           return color;
        case TonemapOperatorSDR::Linear:   return color;
        case TonemapOperatorSDR::Aces:     return TonemapACES(color);
        case TonemapOperatorSDR::Reinhard: return TonemapReinhard(color);
        case TonemapOperatorSDR::AgX:      return TonemapAgX(color);
        case TonemapOperatorSDR::GT7:      return TonemapGT7(color);
    }
}

public float3 TonemapHDR(float3 luminance, TonemapOperatorHDR op, float maxNits, float exposure)
{
    luminance *= exposure;

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