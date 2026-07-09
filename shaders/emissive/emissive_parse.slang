import structs;
import helpers;
import lights;
import emissive_common;

struct Data
{
    float4 modelRow0;
    float4 modelRow1;
    float4 modelRow2;
    uint numTris;
    uint materialIndex;
    uint _pad0;
    uint _pad1;
};

StructuredBuffer<Vertex> vertices       : register(t0, space0);
StructuredBuffer<uint> indices          : register(t1, space0);
StructuredBuffer<Material> materials    : register(t2, space0);

RWStructuredBuffer<Light> lights        : register(u0, space0);
RWStructuredBuffer<Counters> counters   : register(u1, space0);
RWStructuredBuffer<float> powerBuffer   : register(u2, space0);

ConstantBuffer<Data> data               : register(b0, space1);

SamplerState pointSampler               : register(s0, space0);


float3 Transform(float3 p)
{
    float4 h = float4(p, 1.0);
    return float3(dot(data.modelRow0, h),
                  dot(data.modelRow1, h),
                  dot(data.modelRow2, h));
}

[shader("compute")]
[numthreads(32, 1, 1)]
void ParseTriangles(uint3 dtid : SV_DispatchThreadID)
{
    uint triIdx = dtid.x;
    if (triIdx >= data.numTris) return;

    uint i0 = indices[triIdx * 3 + 0];
    uint i1 = indices[triIdx * 3 + 1];
    uint i2 = indices[triIdx * 3 + 2];

    Vertex v0 = vertices[i0];
    Vertex v1 = vertices[i1];
    Vertex v2 = vertices[i2];
    float3 w0 = Transform(v0.position);
    float3 w1 = Transform(v1.position);
    float3 w2 = Transform(v2.position);

    float3 e1 = w1 - w0;
    float3 e2 = w2 - w0;

    Material material = materials[data.materialIndex];

    static const int samples = 12;
    float power = 0.0;
    for (int j = 0; j < samples; j++)
    {
        for (int i = 0; i < samples; i++)
        {
            float u = (i + 0.5) / float(samples);
            float v = (j + 0.5) / float(samples);
            if (u + v > 1.0)
            {
                u = 1.0 - u;
                v = 1.0 - v;
            }
            float w = 1.0 - u - v;
            float2 uv = v0.uv * w + v1.uv * u + v2.uv * v;

            float3 emissive = sampleOrDefault(material.emissiveIndex, pointSampler, uv, material.emissiveFactor);
            power += Grayscale(emissive);
        }
    }
    float area = length(cross(w1 - w0, w2 - w0)) * 0.5;
    power /= samples * samples;
    power *= area;

    if (power > 0.0)
    {
        uint curElement;
        InterlockedAdd(counters[0].numLights, 1, curElement);

        lights[curElement].SetTriV0(w0);
        lights[curElement].SetTriE1(e1);
        lights[curElement].SetTriE2(e2);
        lights[curElement].SetTriMatIdx(data.materialIndex);
        lights[curElement].SetTriUV0(v0.uv);
        lights[curElement].SetTriUV1(v1.uv);
        lights[curElement].SetTriUV2(v2.uv);
        lights[curElement].SetType(LIGHT_TYPE_TRIANGLE);

        powerBuffer[curElement] = power;

        float oldPower = counters[0].totalPower;
        float originalPower;
        [allow_uav_condition]
        while (true)
        {
            float newPower = oldPower + power;
            InterlockedCompareExchangeFloatBitwise(counters[0].totalPower, oldPower, newPower, originalPower);
            if (asuint(originalPower) == asuint(oldPower))
            {
                break;
            }
            oldPower = originalPower;
        }
    }
}

[shader("compute")]
[numthreads(32, 1, 1)]
void ParseLights(uint3 dtid: SV_DispatchThreadID)
{
    // repurposed numTris
    if (dtid.x > data.modelRow0.x) return;

    uint curElement;
    InterlockedAdd(counters[0].numLights, 1, curElement);

    // Read existing light entry instead of creating a new one
    Light light = lights[curElement];
    uint lightType = light.GetType();

    float power = 0.0;
    if (lightType == LIGHT_TYPE_DIRECTIONAL)
    {
        power = Grayscale(light.DirColor()) * 683.0;
    }
    else if (lightType == LIGHT_TYPE_POINT)
    {
        power = Grayscale(light.PointColor()) * 4.0 * 3.14159265;
    }

    powerBuffer[curElement] = power;

    float oldPower = counters[0].totalPower;
    float originalPower;
    [allow_uav_condition]
    while (true)
    {
        float newPower = oldPower + power;
        InterlockedCompareExchangeFloatBitwise(counters[0].totalPower, oldPower, newPower, originalPower);
        if (asuint(originalPower) == asuint(oldPower))
        {
            break;
        }
        oldPower = originalPower;
    }
}