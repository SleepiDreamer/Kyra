import structs;
import helpers;
import settings;
import dof_common;

ConstantBuffer<PostProcessSettings> postProcessSettings : register(b0, space0);
SamplerState linearSampler : register(s0, space0);

[shader("compute")]
[numthreads(8, 8, 1)]
void DownscalePass(uint3 dtid : SV_DispatchThreadID)
{
    Texture2D<float4>   inputTex  = DescriptorHandle<Texture2D>(uint2(inputIdx, 0));
    RWTexture2D<float4> outputTex = DescriptorHandle<RWTexture2D>(uint2(outputIdx, 0));

    //uint2 outputRes;
    //outputTex.GetDimensions(outputRes.x, outputRes.y);
    //uint2 downscaledRes;
    //nearCocTex.GetDimensions(downscaledRes.x, downscaledRes.y);
    //if (dtid.x >= downscaledRes.x || dtid.y >= downscaledRes.y) return;
    //float2 uv = float2(dtid.xy + 0.5) / outputRes.xy;

    uint2 outputRes;
    outputTex.GetDimensions(outputRes.x, outputRes.y);
    float2 uv = float2(dtid.xy + 0.5) / outputRes.xy;
    float2 offsetScale = 1.0 / outputRes;

    float4 CoCs[4];
    CoCs[0] = inputTex.SampleLevel(linearSampler, uv, 0);
    CoCs[1] = inputTex.SampleLevel(linearSampler, uv + float2(1.0, 0.0) * offsetScale, 0);
    CoCs[2] = inputTex.SampleLevel(linearSampler, uv + float2(0.0, 1.0) * offsetScale, 0);
    CoCs[3] = inputTex.SampleLevel(linearSampler, uv + float2(1.0, 1.0) * offsetScale, 0);

    outputTex[dtid.xy] = max(CoCs[0], max(CoCs[1], max(CoCs[2], CoCs[3])));
}