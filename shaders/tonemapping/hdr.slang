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

float3 saturation(float3 color, float strength)
{
    float luma = dot(color, float3(0.299, 0.587, 0.114));
    return lerp(luma.xxx, color, strength);
}

public float3 tonemapHDRLinear(float3 luminance, float maxNits)
{
    luminance *= 0.05;

    float3 wcg = Rec709ToRec2020(luminance);
    float3 nits = wcg * maxNits;
    nits = saturation(nits, 1.1);
    nits = SoftShoulder(nits, maxNits * 0.5, maxNits);

    return nits;
}