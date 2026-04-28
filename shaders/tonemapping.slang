module tonemapping;

import structs;
import tonemapping.aces;
import tonemapping.reinhard;
import tonemapping.agx;
import tonemapping.gt7;
import tonemapping.psycho;
import tonemapping.hdr;

public float3 tonemapSDR(float3 color, TonemapOperator op, float exposure)
{
    color *= exposure;

    switch (op)
    {
        case TonemapOperator::Linear:   return color;
        case TonemapOperator::Aces:     return tonemapACES(color);
        case TonemapOperator::Reinhard: return tonemapReinhard(color);
        case TonemapOperator::AgX:      return tonemapAgX(color);
        case TonemapOperator::GT7:      return tonemapGT7(color);
        default:                        return color;
    }
}

public float3 tonemapHDR(float3 luminance, TonemapOperator op, float paperWhite, float maxNits, float exposure)
{
    luminance *= exposure;
    
    switch (op)
    {
    case TonemapOperator::HDR: return tonemapHDRPQ(luminance, paperWhite, maxNits);
    case TonemapOperator::Psycho: return tonemapPsycho(luminance, paperWhite, maxNits);
    default:                        return luminance;
    }

}