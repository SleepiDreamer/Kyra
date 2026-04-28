module hdr;

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

float3 Rec709ToRec2020(float3 color)
{
    const float3x3 conversion = float3x3(
        0.6274, 0.3293, 0.0433,
        0.0691, 0.9195, 0.0114,
        0.0164, 0.0880, 0.8956
    );
    return mul(conversion, color);
}

public float3 tonemapHDRPQ(float3 luminance, float paperWhite, float maxNits)
{
    luminance *= 0.05; // Normalize brightness across operators
    float3 wcg = Rec709ToRec2020(luminance);
    float3 nits = wcg * maxNits;

    float3 pq = LinearToPQ(nits);

    return pq;
}