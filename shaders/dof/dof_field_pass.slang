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
    Texture2D<float4>   colorTex   = DescriptorHandle<Texture2D>(uint2(colorIdx, 0));
    Texture2D<float>    depthTex   = DescriptorHandle<Texture2D<float>>(uint2(depthIdx, 0));
    RWTexture2D<float4> nearTex    = DescriptorHandle<RWTexture2D>(uint2(nearIdx, 0));
    RWTexture2D<float>  nearCocTex = DescriptorHandle<RWTexture2D<float>>(uint2(nearCoCIdx, 0));
    RWTexture2D<float4> farTex     = DescriptorHandle<RWTexture2D>(uint2(farIdx, 0));

    uint2 resolution;
    farTex.GetDimensions(resolution.x, resolution.y);
    if (dtid.x >= resolution.x || dtid.y >= resolution.y) return;
    float2 uv = float2(dtid.xy + 0.5) / resolution.xy;

    float3 color = colorTex.SampleLevel(linearSampler, uv, 0).rgb;
#if 1 // Use maximum depth texel
    float4 depthSamples = depthTex.GatherRed(linearSampler, uv, 0);
    float depth = max(depthSamples.x, max(depthSamples.y, max(depthSamples.z, depthSamples.w)));
#else
    float depth = depthTex.SampleLevel(linearSampler, uv, 0).r;
#endif

    const float focusDist = postProcessSettings.focusDistance;
    const float lensRadius = postProcessSettings.aperture * 0.01;
    const float fov = float(inputIdx) / 100.0;
    float coc = CalculateCircleOfConfusion(focusDist, depth, lensRadius, fov);
    coc *= resolution.y;

    if (depth < focusDist) // near
    {
        nearTex[dtid.xy] = float4(color, coc);
        nearCocTex[dtid.xy] = coc;
        farTex[dtid.xy] = 0.0;
    }
    else if (depth >= focusDist + 0.0) // far, TODO: focal region
    {
        nearTex[dtid.xy] = float4(0.0, 0.0, 0.0, 1.0);
        nearCocTex[dtid.xy] = 0.0;
        farTex[dtid.xy] = float4(color, coc);
    }
}