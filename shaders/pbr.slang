module pbr;

import settings;
import helpers;
import rng;
import random.sampling;

static const float PI = 3.141592653589f;

public struct PBR
{
    float3 albedo;
    float roughness;
    float3 emission;
    float metallic;
    float3 normal;
    float2 uv;
};

// from "Efficient Construction of Perpendicular Vectors Without Branching"
float3 GetPerpendicularVector(float3 u)
{
    float3 a = abs(u);
    uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
    uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
    uint zm = 1 ^ (xm | ym);
    return normalize(cross(u, float3(xm, ym, zm)));
}

float GGXNormalDistribution(float NdotH, float alpha)
{
    float a2 = alpha * alpha;
    float d = ((NdotH * a2 - NdotH) * NdotH + 1);
    return a2 / (d * d * PI);
}

float GGXSmithG(float NdotX, float roughness)
{
    float a2 = roughness * roughness;
    float denom = NdotX + sqrt(a2 + (1.0 - a2) * NdotX * NdotX);
    return 2.0 * NdotX / denom;
}

float3 Fresnel(float3 f0, float LdotH)
{
    return f0 + (float3(1.0f, 1.0f, 1.0f) - f0) * pow(1.0f - LdotH, 5.0f);
}

// From "Sampling the GGX Distribution of Visible Normals" - Heitz 2018
float3 SampleGGXVNDF(float3 Ve, float alpha, float2 rand)
{
    float3 Vh = normalize(float3(alpha * Ve.x, alpha * Ve.y, Ve.z));

    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1 = lensq > 0 ? float3(-Vh.y, Vh.x, 0) / sqrt(lensq) : float3(1, 0, 0);
    float3 T2 = cross(Vh, T1);

    float r = sqrt(rand.x);
    float phi = 2.0 * PI * rand.y;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;

    return normalize(float3(alpha * Nh.x, alpha * Nh.y, max(0.0, Nh.z)));
}

// PDF: G1(v) * D * VdotH / (4 * NdotV * LdotH)
float3 GetGGXMicrofacet(inout RNG rng, float roughness, float3 n, float3 v)
{
    float2 rand = rng.nextFloat2();

    float3 B = GetPerpendicularVector(n);
    float3 T = cross(B, n);

    float3 Ve = float3(dot(v, T), dot(v, B), dot(n, v));
    float3 hLocal = SampleGGXVNDF(Ve, roughness, rand);

    return T * hLocal.x + B * hLocal.y + n * hLocal.z;
}

public float3 BeersLaw(float3 absorption, float distance)
{
    return exp(-absorption * distance * BEER_ABSORPTION);
}

public float SpecularSelectionProb(float3 n, float3 v, float3 albedo, float metallic)
{
    float NdotV = saturate(dot(n, v));
    float3 F = Fresnel(lerp(0.04, albedo, metallic), NdotV);
    float3 kd = (1.0 - F) * (1.0 - metallic) * albedo;
    float spec = Grayscale(F);
    float diff = Grayscale(kd);
    return clamp(spec / max(spec + diff, 1e-4), 0.05, 0.95);
}

public float3 PBRIndirect(inout RNG rng, float3 n, float3 g, float3 v, float3 albedo, float roughness, float metallic, out float3 l)
{
    float specularChance = SpecularSelectionProb(n, v, albedo, metallic);

    if (rng.nextFloat() < specularChance)
    {
#if 0 // preserve normal map brightness, but lose energy
        l = reflect(-v, h);
        if (dot(g, l) < 0.0)
        {
            l = reflect(l, g);
            h = normalize(v + l);
        }

        if (dot(n, l) <= 0.0) { 
            l = float3(0); return 0.0; 
        }

        float VdotH2 = dot(v, h);
        float3 F2 = Fresnel(F0, VdotH2);
        float NdotL = saturate(dot(n, l));
        float G1_l = GGXSmithG(NdotL, roughness);

        return F2 * G1_l / specularChance;
#else
        // Specular
        float3 h = GetGGXMicrofacet(rng, roughness, n, v);
        if (dot(v, h) <= 0.0) { l = float3(0); return 0.0; }
        l = reflect(-v, h);
#endif
    }
    else
    {
        // Diffuse
        l = tangentToWorld(sampleCosineHemisphere(rng.nextFloat2()), n);
    }

    if (dot(n, l) <= 0.0 || dot(g, l) <= 0.0) { l = float3(0); return 0.0; }

    float pdf = PBRIndirectPdf(n, v, l, roughness, metallic, albedo);
    if (pdf <= 1e-9) { l = float3(0); return 0.0; }

    return PBRDirect(n, l, v, albedo, roughness, metallic) / pdf;
}

public float PBRIndirectPdf(float3 n, float3 v, float3 l, float roughness, float metallic, float3 albedo)
{
    float NdotL = saturate(dot(n, l));
    float NdotV = max(dot(n, v), 0.001);
    if (NdotL <= 0.0) return 0.0;

    float3 h = normalize(v + l);
    float NdotH = saturate(dot(n, h));

    float specularChance = SpecularSelectionProb(n, v, albedo, metallic);

    float D = GGXNormalDistribution(NdotH, roughness);
    float G1_v = GGXSmithG(NdotV, roughness);
    float p_spec = G1_v * D / (4.0 * NdotV); // GGX VNDF solid-angle PDF for l
    float p_diff = NdotL / PI;

    return specularChance * p_spec + (1.0 - specularChance) * p_diff;
}

public float3 PBRTransmission(inout RNG rng, float3 n, float3 g, float3 v, float3 albedo, float eta, float dist, out float3 l, out float3 offsetN)
{
    float NdotV = dot(n, v);
    float3 F = Fresnel(0.04, NdotV);
    float Favg = (F.x + F.y + F.z) / 3.0;

    float sinTheta = sqrt(max(0.0, 1.0 - NdotV * NdotV));
    bool TIR = (eta * sinTheta) > 1.0;
    float specularChance = TIR ? 1 : Favg;

    if (rng.nextFloat() < specularChance)
    {
        // Reflect
        l = reflect(-v, n);
        offsetN = g;
        return float3(1);
    }
    else
    {
        // Refract
        l = normalize(refract(-v, n, eta));
        offsetN = -g;
        return BeersLaw(1 - albedo, dist);
    }
}

public float3 PBRDirect(float3 n, float3 l, float3 v, float3 albedo, float roughness, float metallic)
{
    float NdotL = dot(n, l);
    if (NdotL <= 0.0)
        return 0.0;

    float3 h = normalize(l + v);
    float NdotV = max(dot(n, v), 0.001);
    float NdotH = saturate(dot(n, h));
    float LdotH = saturate(dot(l, h));

    float3 F0 = lerp(0.04, albedo, metallic);
    float3 diffuseColor = (1.0 - metallic) * albedo;

    float3 F = Fresnel(F0, LdotH);

    // The diffuse lobe may only carry the energy the specular lobe did not reflect.
    // Using (1 - F(LdotH)) here is wrong: it is a per-light-direction Fresnel that stays
    // near (1 - F0) over most of the hemisphere, so at grazing angles the specular lobe
    // reflects ~70% of the incoming energy while diffuse still passes ~95% of the albedo
    // through - a directional albedo of up to 1.44 on smooth dielectrics. Weight by the
    // specular lobe's actual directional albedo instead so the two lobes share one budget.
    float3 kd = 1.0 - EnvBRDFApprox2(F0, roughness, NdotV);

    float3 diffuse = kd * diffuseColor / PI;

    float D = GGXNormalDistribution(NdotH, roughness);
    float G = GGXSmithG(NdotL, roughness) * GGXSmithG(NdotV, roughness);
    float3 specular = D * G * F / (4.0 * NdotL * NdotV);

    return (diffuse + specular) * NdotL;
}