import structs;
import helpers;
import settings;
import dof_common;

ConstantBuffer<PostProcessSettings> postProcessSettings : register(b0, space0);
SamplerState linearSampler : register(s0, space0);

[shader("compute")]
[numthreads(8, 8, 1)]
void CombinePass(uint3 dtid : SV_DispatchThreadID)
{
    Texture2D<float4>   colorTex   = DescriptorHandle<Texture2D>(uint2(colorIdx, 0));
    Texture2D<float>    depthTex   = DescriptorHandle<Texture2D<float>>(uint2(depthIdx, 0));
    RWTexture2D<float>  nearCocTex = DescriptorHandle<RWTexture2D<float>>(uint2(nearCoCIdx, 0));
    RWTexture2D<float4> farTex     = DescriptorHandle<RWTexture2D>(uint2(farIdx, 0));
    RWTexture2D<float4> outputTex  = DescriptorHandle<RWTexture2D>(uint2(outputIdx, 0));

    uint2 resolution;
    outputTex.GetDimensions(resolution.x, resolution.y);
    if (dtid.x >= resolution.x || dtid.y >= resolution.y) return;
    float2 uv = float2(dtid.xy + 0.5) / resolution.xy;

    outputTex[dtid.xy] = colorTex[dtid.xy];
}