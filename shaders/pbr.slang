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

public float3 PBRIndirect(inout RNG rng, float3 n, float3 g, float3 v, float3 albedo, float roughness, float metallic, out float3 l)
{
    float3 F0 = lerp(0.04, albedo, metallic);
    float3 diffuseColor = (1.0 - metallic) * albedo;

    float3 h = GetGGXMicrofacet(rng, roughness, n, v);
    float VdotH = dot(v, h);
    if (VdotH < 0.0)
    {
        l = float3(0);
        return 0.0;
    }

    float3 F = Fresnel(F0, VdotH);

    float specularChance = (F.r + F.g + F.b) / 3.0;

    if (rng.nextFloat() < specularChance)
    {
        float2 rand = rng.nextFloat2();

        float3 B = GetPerpendicularVector(n);
        float3 T = cross(B, n);

        float3 Ve = float3(dot(v, T), dot(v, B), dot(n, v));
        float3 hLocal = SampleGGXVNDF(Ve, roughness, rand);

        float3 h = T * hLocal.x + B * hLocal.y + n * hLocal.z;
        l = reflect(-v, h);

        if (dot(l, g) < 0.0)
        {
            l = reflect(l, g);
            h = normalize(v + l);
        }

        float NdotL = saturate(dot(n, l));
        if (NdotL <= 0.0)
            return 0.0;

        if (dot(g, n) * dot(g, l) < 0.0)
            return 0.0;

        float G1_l = GGXSmithG(NdotL, roughness);

        float3 singleScatter = F * G1_l;

        return saturate(singleScatter / specularChance);
    }
    else
    {
        // Diffuse
        l = tangentToWorld(sampleCosineHemisphere(rng.nextFloat2()), n);

        if (dot(l, g) < 0.0)
            l = reflect(l, g);

        if (dot(g, n) * dot(g, l) < 0.0)
            return 0.0;

        float3 transmitted = (1.0 - F) * diffuseColor;
        return saturate(transmitted / (1.0 - specularChance));
    }

    // BRDF: D * G * F / (4 * NdotL * NdotV)
    // PDF:  G(v) * D * VdotH / (NdtoV * 4 * VdotH)
    //     = G(v) * D / (4 * NdotV)
    //
    // Contribution: BRDF * NdotL / PDF
    //             = F * G(l)
}

public float3 PBRTransmission(inout RNG rng, float3 n, float3 v, float3 albedo, float eta, float dist, out float3 l, out float3 offsetN)
{
    float NdotV = dot(n, v);
    float3 F = Fresnel(0.04, NdotV);
    float Favg = (F.x + F.y + F.z) / 3.0;

    float sinTheta = sqrt(max(0.0, 1.0 - NdotV * NdotV));
    bool TIR = (eta * sinTheta) > 1.0;
    float specularChance = TIR ? 1 : Favg;

    if (rng.nextFloat() < specularChance)
    {
        l = reflect(-v, n);
        offsetN = n;
        return F / specularChance;
    }
    else
    {
        l = normalize(refract(-v, n, eta));
        offsetN = -n;
        return BeersLaw(1 - albedo, dist) * (1.0 - F) / max(1.0 - specularChance, 1e-4);
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

    float3 diffuse = (1.0 - F) * diffuseColor / PI;

    float D = GGXNormalDistribution(NdotH, roughness);
    float G = GGXSmithG(NdotL, roughness) * GGXSmithG(NdotV, roughness);
    float3 specular = D * G * F / (4.0 * NdotL * NdotV);

    return (diffuse + specular) * NdotL;
}