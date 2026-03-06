module sampling;

static const float PI      = 3.14159265358979323846f;
static const float TWO_PI  = 6.28318530717958647692f;
static const float INV_PI  = 0.31830988618379067154f;
static const float INV_2PI = 0.15915494309189533577f;

public struct DirectionSample
{
    float3 dir;
    float  pdf;
};

public DirectionSample sampleCosineHemispherePdf(float2 rand)
{
    float2 d = sampleDisk(rand);
    float z = sqrt(max(0.0f, 1.0f - d.x * d.x - d.y * d.y));

    DirectionSample s;
    s.dir = float3(d.x, d.y, z);
    s.pdf = z * INV_PI;
    return s;
}

public float3 sampleCosineHemisphere(float2 rand)
{
    float2 d = sampleDisk(rand);
    float z = sqrt(max(0.0f, 1.0f - d.x * d.x - d.y * d.y));

    return float3(d.x, d.y, z);
}

public float pdfCosineHemisphere(float cosTheta)
{
    return max(0.0f, cosTheta) * INV_PI;
}

public DirectionSample sampleHemispherePdf(float2 rand)
{
    float z = rand.x;
    float r = sqrt(max(0.0f, 1.0f - z * z));
    float phi = TWO_PI * rand.y;

    DirectionSample s;
    s.dir = float3(r * cos(phi), r * sin(phi), z);
    s.pdf = INV_2PI;
    return s;
}

public float3 sampleHemisphere(float2 rand)
{
    float z = rand.x;
    float r = sqrt(max(0.0f, 1.0f - z * z));
    float phi = TWO_PI * rand.y;

    return float3(r * cos(phi), r * sin(phi), z);
}

public float pdfUniformHemisphere()
{
    return INV_2PI;
}

public DirectionSample sampleUniformSpherePdf(float2 rand)
{
    float z = 1.0f - 2.0f * rand.x;
    float r = sqrt(max(0.0f, 1.0f - z * z));
    float phi = TWO_PI * rand.y;

    DirectionSample s;
    s.dir = float3(r * cos(phi), r * sin(phi), z);
    s.pdf = INV_2PI * 0.5f; // 1 / (4*pi)
    return s;
}

public float3 sampleUniformSphere(float2 rand)
{
    float z = 1.0f - 2.0f * rand.x;
    float r = sqrt(max(0.0f, 1.0f - z * z));
    float phi = TWO_PI * rand.y;

    return float3(r * cos(phi), r * sin(phi), z);
}

public float pdfUniformSphere()
{
    return INV_2PI * 0.5f;
}

public float2 sampleDisk(float2 rand)
{
    float2 offset = 2.0f * rand - 1.0f;

    if (offset.x == 0.0f && offset.y == 0.0f)
        return float2(0.0f);

    float theta, r;
    if (abs(offset.x) > abs(offset.y))
    {
        r = offset.x;
        theta = (PI / 4.0f) * (offset.y / offset.x);
    }
    else
    {
        r = offset.y;
        theta = (PI / 2.0f) - (PI / 4.0f) * (offset.x / offset.y);
    }
    return r * float2(cos(theta), sin(theta));
}

public float3 sampleSun(float2 rand, float3 sunDir, float radius)
{
    float r = sqrt(rand.x) * radius;
    float theta = 2.0 * PI * rand.y;
    float x = r * cos(theta);
    float y = r * sin(theta);

    float3 tangent = abs(sunDir.y) < 0.999
        ? normalize(cross(float3(0, 1, 0), sunDir))
        : normalize(cross(float3(1, 0, 0), sunDir));
    float3 bitangent = cross(sunDir, tangent);

    return normalize(sunDir + tangent * x + bitangent * y);
}

public float2 sampleUniformTriangle(float2 rand)
{
    float su = sqrt(rand.x);
    return float2(1.0f - su, rand.y * su);
}

public DirectionSample sampleUniformConePdf(float2 rand, float cosThetaMax)
{
    float cosTheta = 1.0f - rand.x * (1.0f - cosThetaMax);
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = TWO_PI * rand.y;

    DirectionSample s;
    s.dir = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    s.pdf = pdfUniformCone(cosThetaMax);
    return s;
}

public float3 sampleUniformCone(float2 rand, float cosThetaMax)
{
    float cosTheta = 1.0f - rand.x * (1.0f - cosThetaMax);
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = TWO_PI * rand.y;

    return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

public float pdfUniformCone(float cosThetaMax)
{
    return 1.0f / (TWO_PI * (1.0f - cosThetaMax));
}

public DirectionSample sampleGGX(float2 xi, float alpha)
{
    float a2 = alpha * alpha;
    float cosTheta = sqrt(max(0.0f, (1.0f - xi.x) / (xi.x * (a2 - 1.0f) + 1.0f)));
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = TWO_PI * xi.y;

    DirectionSample s;
    s.dir = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    s.pdf = pdfGGX(cosTheta, alpha);
    return s;
}

public float pdfGGX(float cosTheta, float alpha)
{
    float a2 = alpha * alpha;
    float d = cosTheta * cosTheta * (a2 - 1.0f) + 1.0f;
    float D = a2 * INV_PI / (d * d);
    return D * cosTheta;
}

public void buildTBN(float3 n, out float3 t, out float3 b)
{
    if (n.z < -0.9999999f)
    {
        t = float3(0.0f, -1.0f, 0.0f);
        b = float3(-1.0f, 0.0f, 0.0f);
        return;
    }
    float a = 1.0f / (1.0f + n.z);
    float d = -n.x * n.y * a;
    t = float3(1.0f - n.x * n.x * a, d, -n.x);
    b = float3(d, 1.0f - n.y * n.y * a, -n.y);
}

public float3 tangentToWorld(float3 localDir, float3 n)
{
    float3 t, b;
    buildTBN(n, t, b);
    return t * localDir.x + b * localDir.y + n * localDir.z;
}