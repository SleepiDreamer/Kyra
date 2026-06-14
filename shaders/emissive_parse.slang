import structs;
import helpers;
import lights;

struct Data
{
    float4 modelRow0;
    float4 modelRow1;
    float4 modelRow2;
    uint numTris;
    uint numElements;
    uint materialIndex;
};

StructuredBuffer<Vertex> vertices       : register(t0, space0);
StructuredBuffer<uint> indices          : register(t1, space0);
StructuredBuffer<Material> materials    : register(t2, space0);
RWStructuredBuffer<Light> lights        : register(u0, space0);
RWStructuredBuffer<uint> lightCounter   : register(u1, space0);
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
[numthreads(64, 1, 1)]
void ParseEmissives(uint3 dtid : SV_DispatchThreadID)
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

    float2 uv = v0.uv * 0.5 + v1.uv * 0.5;
    float3 emissive = sampleOrDefault(material.emissiveIndex, pointSampler, uv, material.emissiveFactor);
    float area = length(cross(w1 - w0, w2 - w0)) * 0.5;
    float power = Grayscale(emissive) * area;

    if (power > 0.0)
    {
        uint curElement;
        InterlockedAdd(lightCounter[0], 1, curElement);

        lights[curElement].SetTriArea(area);
        lights[curElement].SetTriE1(e1);
        lights[curElement].SetTriE2(e2);
        lights[curElement].SetTriMaterialIdx(material.emissiveIndex);
        lights[curElement].SetTriNormal(normalize(cross(e1, e2)));
        lights[curElement].SetTriV0(w0);
        lights[curElement].SetType(LIGHT_TYPE_TRIANGLE);
    }
}