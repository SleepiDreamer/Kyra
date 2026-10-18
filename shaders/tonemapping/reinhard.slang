module reinhard;
import helpers;

public float3 TonemapReinhard(float3 luminance)
{
    luminance = max(luminance, 0.0);
    luminance = luminance / (1.0 + luminance);
    luminance = ACEScgToRec709(luminance);
    return luminance;
}