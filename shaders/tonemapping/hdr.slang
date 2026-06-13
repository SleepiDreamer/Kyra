module hdr;

public float3 LinearToPQ(float3 linearNits)
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

public float3 PQToLinear(float3 pq)
{
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;

    float3 Lm2 = pow(pq, 1.0 / m2);
    float3 numerator = max(Lm2 - c1, 0.0);
    float3 denominator = c2 - c3 * Lm2;

    return pow(numerator / denominator, 1.0 / m1) * 10000.0;
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

float3 SoftShoulder(float3 L_nits, float knee, float peak)
{
    float3 result;
    for (int i = 0; i < 3; i++) {
        float x = L_nits[i];
        if (x <= knee) 
        {
            result[i] = x;
        } 
        else
        {
            float excess = x - knee;
            float range = peak - knee;
            result[i] = knee + range * (excess / (excess + range));
        }
    }
    return result;
}

float S_Curve(float x, float k)
{
    float fx = 1.0 / (1.0 + exp(-k * (x - 0.5)));
    float f0 = 1.0 / (1.0 + exp(k * 0.5));
    float f1 = 1.0 / (1.0 + exp(-k * 0.5));
    return (fx - f0) / (f1 - f0);
}

float HighlightDesaturation(float x, float m, float a, float b, float k)
{
    x = clamp(x, 0, m);
    return a + (b - a) / (1.0 + exp(-k * (x - m / 2.0)));
}

float3 Saturation(float3 color, float strength)
{
    float luma = dot(color, float3(0.299, 0.587, 0.114));
    return lerp(luma.xxx, color, strength);
}

public float3 TonemapHDRLinear(float3 luminance, float maxNits)
{
    luminance *= 0.05;

    float3 wcg = Rec709ToRec2020(luminance);
    float3 nits = wcg * maxNits;
    
    return nits;
}

public float3 TonemapHDRCustom(float3 luminance, float maxNits)
{
    luminance *= 0.05;

    float3 wcg = Rec709ToRec2020(luminance);
    float3 nits = wcg * maxNits;

    float maxComponent = max(nits.x, max(nits.y, nits.z));
    float saturation = HighlightDesaturation(maxComponent, maxNits, 1.2, 0.8, 0.01);
    nits = Saturation(nits, saturation);

    float contrast = 2.0;
    float3 PQ = LinearToPQ(nits);
    float luma = dot(PQ, float3(0.2627, 0.6780, 0.0593));
    PQ *= S_Curve(luma, max(contrast, 0.01)) / max(luma, 0.0001);
    nits = PQToLinear(PQ);

    float rolloff = 0.7;
    nits = SoftShoulder(nits, maxNits * (1.0 - rolloff), maxNits);

    return nits;
}