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

    public uint GetType() { return asuint(this.data0.w) >> 24; }
    [mutating] public void SetType(uint type) { this.data0.w = asfloat((asuint(this.data0.w) & 0x00FFFFFF) | (type << 24)); }

    [ForceInline] public float3 DirDirection()  { return this.data0.xyz; }
    [ForceInline] public float3 DirColor()      { return this.data1.xyz; }
    [ForceInline] public float DirAngularSize() { return this.data1.w; }

    [ForceInline] public float3 PointPos()  { return this.data0.xyz; }
    [ForceInline] public float3 PointColor(){ return this.data1.xyz; }
    [ForceInline] public float PointSize()  { return this.data1.w; }

    [ForceInline] public float3 TriV0()     { return this.data0.xyz; }
    [ForceInline] public float3 TriE1()     { return this.data1.xyz; }
    [ForceInline] public float3 TriE2()     { return this.data2.xyz; }
    [ForceInline] public uint TriMatIdx()   { return asuint(this.data0.w) & 0x00FFFFFF; }
    [ForceInline] public float2 TriUV0()    { return float2(this.data1.w, this.data2.w); }
    [ForceInline] public float2 TriUV1()    { return float2(this.data3.x, this.data3.y); }
    [ForceInline] public float2 TriUV2()    { return float2(this.data3.z, this.data3.w); }
    [ForceInline] public float3 TriNormal() { return normalize(cross(TriE1(), TriE2())); }

    [ForceInline] [mutating] public void SetTriV0(float3 v0) { this.data0 = float4(v0, this.data0.w); }
    [ForceInline] [mutating] public void SetTriE1(float3 e1) { this.data1 = float4(e1, this.data1.w); }
    [ForceInline] [mutating] public void SetTriE2(float3 e2) { this.data2 = float4(e2, this.data2.w); }
    [ForceInline] [mutating] public void SetTriMatIdx(uint idx) { this.data0.w = asfloat((asuint(this.data0.w) & 0xFF000000) | (idx & 0x00FFFFFF)); }    [ForceInline] [mutating] public void SetTriUV0(float2 uv) { this.data1.w = uv.x; this.data2.w = uv.y; }
    [ForceInline] [mutating] public void SetTriUV1(float2 uv) { this.data3.x = uv.x; this.data3.y = uv.y; }
    [ForceInline] [mutating] public void SetTriUV2(float2 uv) { this.data3.z = uv.x; this.data3.w = uv.y; }

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
        float a = 0.5 * length(cross(TriE1(), TriE2()));
        dist = sqrt(d2);
        dirToLight = d / dist;

        SamplerState sampler = DescriptorHandle<SamplerState>(uint2(material.samplerIndex, 0));
        float2 uv = TriUV0() * b0 + TriUV1() * b1 + TriUV2() * b2;
        float3 radiance = sampleOrDefault(material.emissiveIndex, sampler, uv, material.emissiveFactor);

        return radiance * float3(abs(dot(n, -dirToLight)) * a / d2);
    }
};