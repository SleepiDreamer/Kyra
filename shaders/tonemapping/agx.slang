module agx;

// Original sigmoid fit: MIT License, Copyright (c) 2024 Missing Deadlines (Benjamin Wrensch). 
// Matrices from Blender's config.ocio / EaryChow's AgX.

// 0: None (Blender default), 1: Golden, 2: Punchy
#define AGX_LOOK 0

static const float3x3 srgb_to_rec2020 = float3x3(
    0.6274038959346990, 0.3292830383778838, 0.0433130656874172,
    0.0690972893582320, 0.9195403950754586, 0.0113623155663092,
    0.0163914388751502, 0.0880133078772258, 0.8955952532476240);

static const float3x3 rec2020_to_srgb = float3x3(
    1.6604910021084343, -0.5876411387885497, -0.0728498633198849,
   -0.1245504745215906,  1.1328998971259605, -0.0083494226043695,
   -0.0181507633549052, -0.1005788980080074,  1.1187296613629125);

static const float3x3 agx_inset = float3x3(
    0.856627153315983, 0.0951212405381588, 0.0482516061458583,
    0.137318972929847, 0.7612419906025910, 0.1014390364675620,
    0.111898212999950, 0.0767994186031903, 0.8113023683968590);

static const float3x3 agx_outset = float3x3(
    1.1271005818144368, -0.1106066430966032, -0.0164939387178346,
   -0.1413297634984383,  1.1578237022162720, -0.0164939387178343,
   -0.1413297634984383, -0.1106066430966029,  1.2519364065950405);

static const float min_ev = -12.47393f;
static const float max_ev = 4.026069f;

float3 agxDefaultContrastApprox(float3 x)
{
    float3 x2 = x * x;
    float3 x4 = x2 * x2;

    return +15.5 * x4 * x2
            - 40.14 * x4 * x
            + 31.96 * x4
            - 6.868 * x2 * x
            + 0.4298 * x2
            + 0.1191 * x
            - 0.00232;
}

float3 agx(float3 val)
{
    val = mul(srgb_to_rec2020, val);
    val = max(val, 0.0f);
    val = mul(agx_inset, val);
    val = max(val, 1e-10f);

    val = clamp(log2(val), min_ev, max_ev);
    val = (val - min_ev) / (max_ev - min_ev);

    val = agxDefaultContrastApprox(val);
    return val;
}

float3 agxLook(float3 val)
{
    float3 offset = float3(0.0, 0.0, 0.0);
    float3 slope = float3(1.0, 1.0, 1.0);
    float3 power = float3(1.0, 1.0, 1.0);
    float sat = 1.15;

#if AGX_LOOK == 1
    // Golden
    slope = float3(1.0, 0.95, 0.9);
    power = float3(0.8, 0.8, 0.8);
    sat = 0.8;
#elif AGX_LOOK == 2
    // Punchy
    slope = float3(1.0, 1.0, 1.0);
    power = float3(1.35, 1.35, 1.35);
    sat = 1.4;
#endif

    val = pow(max(val * slope + offset, 0.0f), power);

    static const float3 lw = float3(0.2589235355689848,
                                    0.6104985346066525,
                                    0.1305779298243628);
    float luma = dot(val, lw);
    return luma + sat * (val - luma);
}

float3 agxEotf(float3 val)
{
    val = mul(agx_outset, val);
    val = saturate(val);

    val = pow(val, float3(2.2, 2.2, 2.2));

    val = mul(rec2020_to_srgb, val);
    return max(val, 0.0f);
}

public float3 TonemapAgX(float3 color)
{
    color = agx(color);
    color = agxLook(color);
    color = agxEotf(color);
    return color;
}

float3 Contrast(float3 color, float cont)
{
    return pow(color, cont);
}
