import structs;
import helpers;
import settings;

cbuffer DescriptorIndices : register(b4, space0)
{
    uint inputIdx;
    uint channel; // 0: RGB, 1: A, 2: R
    uint outputIdx;
};

SamplerState linearSampler : register(s0, space0);

[shader("compute")]
[numthreads(8, 8, 1)]
void DebugPass(uint3 dtid : SV_DispatchThreadID)
{
    Texture2D<float4>   inputTex  = DescriptorHandle<Texture2D>(uint2(inputIdx, 0));
    RWTexture2D<float4> outputTex = DescriptorHandle<RWTexture2D>(uint2(outputIdx, 0));

    uint2 resolution;
    outputTex.GetDimensions(resolution.x, resolution.y);
    if (dtid.x >= resolution.x || dtid.y >= resolution.y) return;
    float2 uv = float2(dtid.xy + 0.5) / resolution.xy;

    float3 color;
    switch (channel)
    {
        case 0:
        {
            color = inputTex.SampleLevel(linearSampler, uv, 0).rgb;
            break;
        }
        case 1:
        {
            color = inputTex.SampleLevel(linearSampler, uv, 0).aaa;
            break;
        }
        case 2:
        {
            color = inputTex.SampleLevel(linearSampler, uv, 0).rrr;
            break;
        }
    }

    outputTex[dtid.xy] = float4(color, 1.0);
}