module opendrt;
import helpers;

public float3 TonemapOpenDRTSDR(float3 luminance, Texture3D lut, SamplerState sampler)
{
    luminance *= 1.5; // exposure calibration
    uint3 dims;
    lut.GetDimensions(dims.x, dims.y, dims.z);
    float lutSize = float(dims.x);

    float3 cct = saturate(ACEScgToACEScct(luminance));
    float3 uvw = cct * ((lutSize - 1.0) / lutSize) + 0.5 / lutSize;
    float3 rec709 = lut.SampleLevel(sampler, uvw, 0).rgb;
    return rec709;
}

public float3 TonemapOpenDRTHDR(float3 luminance, float maxNits, Texture3D lut, SamplerState sampler)
{
    luminance *= 0.5; // exposure calibration
    uint3 dims;
    lut.GetDimensions(dims.x, dims.y, dims.z);
    float lutSize = float(dims.x);

    float3 cct = saturate(ACEScgToACEScct(luminance));
    float3 uvw = cct * ((lutSize - 1.0) / lutSize) + 0.5 / lutSize;
    float3 PQ = lut.SampleLevel(sampler, uvw, 0).rgb;
    return PQ;
}