module helpers;

import structs;
import settings;

static const float PI = 3.141592653589f;

public float sanitize(float x)
{
    return isnan(x) || isinf(x) ? 0.0f : x;
}

public float2 sanitize(float2 v) { return float2(sanitize(v.x), sanitize(v.y)); }
public float3 sanitize(float3 v) { return float3(sanitize(v.x), sanitize(v.y), sanitize(v.z)); }
public float4 sanitize(float4 v) { return float4(sanitize(v.x), sanitize(v.y), sanitize(v.z), sanitize(v.w)); }

public float LinearToSrgb(float linear)
{
    if (linear <= 0.0031308)
        return linear * 12.92;
    else
        return 1.055 * pow(linear, 1.0 / 2.4) - 0.055;
}

public float3 LinearToSrgb(float3 linear)
{
    return float3(LinearToSrgb(linear.r), LinearToSrgb(linear.g), LinearToSrgb(linear.b));
}

public float SrgbToLinear(float srgb)
{
    if (srgb <= 0.04045)
        return srgb / 12.92;
    else
        return pow((srgb + 0.055) / 1.055, 2.4);
}

public float2 sampleTriangle(float2 x0, float2 x1, float2 x2, float2 bary)
{
    float w = 1 - bary.x - bary.y;
    return (x0 * w) + (x1 * bary.x) + (x2 * bary.y);
}

public float3 sampleTriangle(float3 x0, float3 x1, float3 x2, float2 bary)
{
    float w = 1 - bary.x - bary.y;
    return (x0 * w) + (x1 * bary.x) + (x2 * bary.y);
}

public float4 sampleTriangle(float4 x0, float4 x1, float4 x2, float2 bary)
{
    float w = 1 - bary.x - bary.y;
    return (x0 * w) + (x1 * bary.x) + (x2 * bary.y);
}

public float3 SrgbToLinear(float3 srgb)
{
    return float3(SrgbToLinear(srgb.r), SrgbToLinear(srgb.g), SrgbToLinear(srgb.b));
}

[ForceInline]
public float3 sampleOrDefault(int texIndex, SamplerState s, float2 uv, float3 factor)
{
    if (texIndex != -1)
    {
        Texture2D tex = DescriptorHandle<Texture2D>(uint2(texIndex, 0));
        return tex.SampleLevel(s, uv, 0).rgb * factor;
    }
    return factor;
}

public float3 OffsetRay(float3 p, float3 n)
{
    const float origin = 1.0f / 32.0f;
    const float float_scale = 1.0f / 65536.0f;
    const float int_scale = 256.0f;

    int3 of_i = int3(int_scale * n);

    float3 p_i = float3(
        asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
                        asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
                        asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return float3(
        abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
                  abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
                  abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
}

public float2 DirectionToEquirectangular(float3 dir)
{
    float u = atan2(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float v = 1.0 - (asin(clamp(dir.y, -1.0, 1.0)) / PI + 0.5);
    return float2(u, v);
}

public float depthToReverseZ(float rayT)
{
    return NEAR_PLANE / max(rayT, NEAR_PLANE);
}

public float depthLinear(float rayT)
{
    return rayT / FAR_PLANE;
}

public float2 WorldToUV(float3 worldPos, CameraData cam)
{
    float3 dir = worldPos - cam.position;

    float z = dot(dir, cam.forward);
    float x = dot(dir, cam.right);
    float y = dot(dir, cam.up);

    if (z <= 0.0)
    { 
        return float2(-1, -1);
    }

    float2 res = DispatchRaysDimensions().xy;
    float aspect = res.x / res.y;
    float tanHalfFov = tan(radians(cam.fov) * 0.5);

    float ndcX = x / (z * aspect * tanHalfFov);
    float ndcY = -y / (z * tanHalfFov);

    return (float2(ndcX, ndcY) + 1.0) * 0.5;
}

// From DLSS-RR integration guide
public float3 EnvBRDFApprox2(float3 SpecularColor, float alpha, float NoV)
{
    NoV = abs(NoV);
    // [Ray Tracing Gems, Chapter 32]
    float4 X;
    X.x = 1.f;
    X.y = NoV;
    X.z = NoV * NoV;
    X.w = NoV * X.z;
    float4 Y;
    Y.x = 1.f;
    Y.y = alpha;
    Y.z = alpha * alpha;
    Y.w = alpha * Y.z;
    float2x2 M1 = float2x2(0.99044f, -1.28514f, 1.29678f, -0.755907f);
    float3x3 M2 = float3x3(1.f, 2.92338f, 59.4188f, 20.3225f, -27.0302f, 222.592f, 121.563f, 626.13f, 316.627f);
    float2x2 M3 = float2x2(0.0365463f, 3.32707, 9.0632f, -9.04756);
    float3x3 M4 = float3x3(1.f, 3.59685f, -1.36772f, 9.04401f, -16.3174f, 9.22949f, 5.56589f, 19.7886f, -20.2123f);
    float bias = dot(mul(M1, X.xy), Y.xy) * rcp(dot(mul(M2, X.xyw), Y.xyw));
    float scale = dot(mul(M3, X.xy), Y.xy) * rcp(dot(mul(M4, X.xzw), Y.xyw));
    // This is a hack for specular reflectance of 0
    bias *= saturate(SpecularColor.g * 50);
    return mad(SpecularColor, max(0, scale), max(0, bias));
}