module hdr;

float3 Rec709ToRec2020(float3 color)
{
    const float3x3 conversion = float3x3(
        0.6274, 0.3293, 0.0433,
        0.0691, 0.9195, 0.0114,
        0.0164, 0.0880, 0.8956
    );
    return mul(conversion, color);
}

float3 SoftShoulder(float3 L_nits, float Lknee, float Lpeak)
{
    // Below knee: pass through unchanged (ground truth preserved)
    // Above knee: compress asymptotically toward Lpeak
    float3 result;
    [unroll] for (int i = 0; i < 3; i++) {
        float x = L_nits[i];
        if (x <= Lknee) {
            result[i] = x;
        } else {
            // Reinhard-style compression of the excess
            float excess = x - Lknee;
            float range = Lpeak - Lknee;
            result[i] = Lknee + range * (excess / (excess + range));
        }
    }
    return result;
}

public float3 tonemapHDRLinear(float3 luminance, float maxNits)
{
    luminance *= 0.1;

    float3 wcg = Rec709ToRec2020(luminance);
    float3 nits = wcg * maxNits;
    nits = SoftShoulder(nits, maxNits * 0.5, maxNits);

    return nits;
}