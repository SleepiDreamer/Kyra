module tonemapping;

import structs;
import tonemapping.aces;
import tonemapping.reinhard;
import tonemapping.agx;
import tonemapping.gt7;
import tonemapping.hdr;

public float3 tonemap(float3 color, TonemapOperator op, float exposure)
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

public float3 tonemapHDR(float3 luminance, float paperWhite, float maxNits, float exposure)
{
    luminance *= exposure;

    return tonemapHDRPQ(luminance, paperWhite, maxNits);
}