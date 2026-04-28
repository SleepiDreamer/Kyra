module tonemapping;

import structs;
import tonemapping.aces;
import tonemapping.reinhard;
import tonemapping.agx;
import tonemapping.gt7;
import tonemapping.psycho;
import tonemapping.hdr;

float3 LinearToPQ(float3 linearNits)
{
    float3 L = linearNits / 10000.0;
    L = max(L, 0.0);

    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;

    float3 Lm1 = pow(L, m1);
    float3 numerator = c1 + c2 * Lm1;
    float3 denominator = 1.0 + c3 * Lm1;

    return pow(numerator / denominator, m2);
}

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

public float3 tonemapHDR(float3 luminance, TonemapOperator op, float maxNits, float exposure)
{
    luminance *= exposure;

    float3 tonemapped;

    switch (op)
    {
    default:                        tonemapped = tonemapHDRLinear(luminance, maxNits); break;
    case TonemapOperator::Linear:   tonemapped = tonemapHDRLinear(luminance, maxNits); break;
    case TonemapOperator::Psycho:   tonemapped = tonemapPsycho(luminance, maxNits); break;
    }

    return LinearToPQ(tonemapped);

}