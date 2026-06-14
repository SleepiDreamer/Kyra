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
    [ForceInline] public float3 TriNormal() { return this.data3.xyz; }
    [ForceInline] public uint TriMaterialIdx() { return asuint(this.data3.w); }

    [ForceInline] [mutating] public void SetTriV0(float3 v0) { this.data0 = float4(v0, this.data0.w); }
    [ForceInline] [mutating] public void SetTriE1(float3 e1) { this.data1 = float4(e1, this.data1.w); }
    [ForceInline] [mutating] public void SetTriArea(float area) { this.data1.w = area; }
    [ForceInline] [mutating] public void SetTriE2(float3 e2) { this.data2 = float4(e2, this.data2.w); }
    [ForceInline] [mutating] public void SetTriNormal(float3 n) { this.data3 = float4(n, this.data3.w); }
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

    public float3 SampleTriangleLight(float2 rand, float3 hitPoint, out float dist, out float3 dirToLight)
    {
        float su = sqrt(rand.x);
        float b1 = 1.0 - su;
        float b2 = rand.y * su;
        float3 p = TriV0() + TriE1() * b1 + TriE2() * b2;
        float3 n = TriNormal();

        float3 d = p - hitPoint;
        float d2 = dot(d, d);
        dist = sqrt(d2);
        dirToLight = d / dist;

        return abs(dot(n, -dirToLight)) / d2; // TODO: add texturing
    }
};