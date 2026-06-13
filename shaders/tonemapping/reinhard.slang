module reinhard;

public float3 TonemapReinhard(float3 color)
{
    color = max(color, 0.0);
    return color / (1.0 + color);
}