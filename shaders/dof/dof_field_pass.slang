import structs;
import helpers;
import settings;
import dof_common;

ConstantBuffer<PostProcessSettings> postProcessSettings : register(b0, space0);
SamplerState linearSampler : register(s0, space0);

[shader("compute")]
[numthreads(8, 8, 1)]
void FieldPass(uint3 dtid : SV_DispatchThreadID)
{
    Texture2D<float4>   colorTex  = DescriptorHandle<Texture2D>(uint2(colorIdx, 0));
    Texture2D<float>    depthTex  = DescriptorHandle<Texture2D<float>>(uint2(depthIdx, 0));
    RWTexture2D<float4> nearTex   = DescriptorHandle<RWTexture2D>(uint2(nearIdx, 0));
    RWTexture2D<float4> farTex    = DescriptorHandle<RWTexture2D>(uint2(farIdx, 0));
    RWTexture2D<float4> outputTex = DescriptorHandle<RWTexture2D>(uint2(outputIdx, 0));

    uint2 resolution;
    outputTex.GetDimensions(resolution.x, resolution.y);
    if (dtid.x >= resolution.x || dtid.y >= resolution.y) return;
    float2 uv = float2(dtid.xy + 0.5) / resolution.xy;

    float3 color = colorTex.SampleLevel(linearSampler, uv, 0).rgb;
    float  depth = depthTex.SampleLevel(linearSampler, uv, 0).r;

    const float focusDist = postProcessSettings.focusDistance;
    const float aperture = postProcessSettings.aperture;
    float coc = CalculateCircleOfConfusion(focusDist, depth, aperture);

    if (coc < 0.0) // near
    {
        nearTex[dtid.xy] = float4(color, abs(coc));
        farTex[dtid.xy] = 0;
    }
    else // far
    {
        nearTex[dtid.xy] = 0;
        farTex[dtid.xy] = float4(color, abs(coc));
    }

    // float3 output = abs(coc).xxx;
    // float3 output = (abs(focusDist - depth) / depth).xxx;

    // float3 output = depth > focusDist ? depth.xxx : 0;
    // output *= coc > 0.0 ? float3(0, 1, 0) : float3(1, 0, 0);

    float4 nearSample = nearTex[dtid.xy].rgba;
    float4 farSample  = farTex[dtid.xy].rgba * 0;

    float3 output = nearSample.rgb * float3(0.5,1.0,0.0) * nearSample.a + farSample.rgb * float3(1.0,0.5,0.5) * farSample.a;

    outputTex[dtid.xy] = float4(output, 1.0);
}