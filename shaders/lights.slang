module lights;

import helpers;
import structs;
import settings;
import random.sampling;

public static const uint LIGHT_TYPE_POINT = 0;
public static const uint LIGHT_TYPE_DIRECTIONAL = 1;
public static const uint LIGHT_TYPE_TRIANGLE = 2;

public struct Light
{
    public float4 data0;
    public float4 data1;
    public float4 data2;
    public float4 data3;

    public uint GetType() { return asuint(this.data0.w); }
    [mutating] public void SetType(uint type) { this.data0.w = asfloat(type); }

    [ForceInline] public float3 DirDirection() { return this.data0.xyz; }
    [ForceInline] public float3 DirColor() { return this.data1.xyz; }
    [ForceInline] public float DirAngularSize() { return this.data1.w; }

    [ForceInline] public float3 PointPos() { return this.data0.xyz; }
    [ForceInline] public float3 PointColor() { return this.data1.xyz; }
    [ForceInline] public float PointSize() { return this.data1.w; }

    [ForceInline] public float3 TriV0() { return this.data0.xyz; }
    [ForceInline] public float3 TriE1() { return this.data1.xyz; }
    [ForceInline] public float TriArea() { return this.data1.w; }
    [ForceInline] public float3 TriE2() { return this.data2.xyz; }
    [ForceInline] public uint TriVtxBufIdx() { return asuint(this.data2.w); }
    [ForceInline] public uint TriIdxBufIdx() { return asuint(this.data3.x); }
    [ForceInline] public uint TriPrimIdx() { return asuint(this.data3.y); }
    [ForceInline] public uint TriMaterialIdx() { return asuint(this.data3.w); }
    [ForceInline] public float3 TriNormal() { return normalize(cross(TriE1(), TriE2())); }

    [ForceInline] [mutating] public void SetTriV0(float3 v0) { this.data0 = float4(v0, this.data0.w); }
    [ForceInline] [mutating] public void SetTriE1(float3 e1) { this.data1 = float4(e1, this.data1.w); }
    [ForceInline] [mutating] public void SetTriArea(float area) { this.data1.w = area; }
    [ForceInline] [mutating] public void SetTriE2(float3 e2) { this.data2 = float4(e2, this.data2.w); }
    [ForceInline] [mutating] public void SetTriVtxBufIdx(uint i) { this.data2.w = asfloat(i); }
    [ForceInline] [mutating] public void SetTriIdxBufIdx(uint i) { this.data3.x = asfloat(i); }
    [ForceInline] [mutating] public void SetTriPrimIdx(uint i) { this.data3.y = asfloat(i); }
    [ForceInline] [mutating] public void SetTriMaterialIdx(uint idx) { this.data3.w = asfloat(idx); }

    public float3 SampleDirectionalLight(float2 rand, out float dist, out float3 dirToLight)
    {
        dist = MAX_RAY_DEPTH;
        dirToLight = sampleSun(rand, -DirDirection(), DirAngularSize() * 4.0);
        return DirColor();
    }

    public float3 SamplePointLight(float2 rand, float3 hitPoint, out float dist, out float3 dirToLight)
    {
        float3 p = PointPos() + sampleUniformSphere(rand) * PointSize();
        dist = length(p - hitPoint);
        dirToLight = normalize(p - hitPoint);
        return max(PointColor() * LightAttenuation(dist), 0);
    }

    public float3 SampleTriangleLight(float2 rand, float3 hitPoint, out float dist, out float3 dirToLight, Material material)
    {
        float su = sqrt(rand.x);
        float b1 = 1.0 - su;
        float b2 = rand.y * su;
        float b0 = 1.0 - b1 - b2;

        float3 p = TriV0() + TriE1() * b1 + TriE2() * b2;
        float3 n = TriNormal();
        float3 d = p - hitPoint;
        float d2 = dot(d, d);
        dist = sqrt(d2);
        dirToLight = d / dist;

        StructuredBuffer<uint> idx = DescriptorHandle<StructuredBuffer<uint>>(uint2(TriIdxBufIdx(), 0));
        StructuredBuffer<Vertex> vtx = DescriptorHandle<StructuredBuffer<Vertex>>(uint2(TriVtxBufIdx(), 0));
        uint t = TriPrimIdx();
        uint i0 = idx[t * 3 + 0], i1 = idx[t * 3 + 1], i2 = idx[t * 3 + 2];
        float2 uv = vtx[i0].uv * b0 + vtx[i1].uv * b1 + vtx[i2].uv * b2;

        SamplerState sampler = DescriptorHandle<SamplerState>(uint2(material.samplerIndex, 0));
        float3 radiance = sampleOrDefault(material.emissiveIndex, sampler, uv, float3(1.0));

        return radiance * float3(abs(dot(n, -dirToLight)) / d2); // TODO: add texturing
    }
};