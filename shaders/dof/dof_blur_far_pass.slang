import structs;
import helpers;
import settings;
import dof_common;

ConstantBuffer<PostProcessSettings> postProcessSettings : register(b0, space0);
SamplerState linearSampler : register(s0, space0);

static const int NUM_TAPS = 8;

[shader("compute")]
[numthreads(8, 8, 1)]
void BlurFarPass(uint3 dtid : SV_DispatchThreadID)
{
    Texture2D<float4> farTex = DescriptorHandle<Texture2D>(uint2(farIdx, 0));
    RWTexture2D<float4> outputTex = DescriptorHandle<RWTexture2D>(uint2(outputIdx, 0));

    uint2 outRes;
    outputTex.GetDimensions(outRes.x, outRes.y);
    uint2 farRes;
    farTex.GetDimensions(farRes.x, farRes.y);
    if (dtid.x >= outRes.x || dtid.y >= outRes.y) return;
    float2 uv = float2(dtid.xy + 0.5) / outRes.xy;

    float4 farFieldPixel = farTex.SampleLevel(linearSampler, uv, 0).rgba;
    float3 pixelColor = farFieldPixel.rgb;
    float pixelCoc = farFieldPixel.a;

    float blurRadius = postProcessSettings.aperture * pixelCoc;

    float3 result = float3(0);
    float weight = 0.0;
    if (pixelCoc > 0.0)
    {
    	for (int u = 0; u < NUM_TAPS; ++u)
    	{
    		for (int v = 0; v < NUM_TAPS; ++v)
    		{
    			float2 sampleUv = float2(u, v) / (NUM_TAPS - 1);
    			sampleUv = SquareToPolygonMapping(sampleUv, 0, 0) / farRes;
    			sampleUv = uv.xy + blurRadius * sampleUv;

    			float4 sampleColor = farTex.SampleLevel(linearSampler, sampleUv, 0);

    			float tapWeight = sampleColor.w * saturate(1.0f - (pixelCoc - sampleColor.w));

    			result +=  sampleColor.xyz * tapWeight;
    			weight += tapWeight;
    		}
    	}
    	if (weight > 0)
    	{
            result /= weight;
        }
    	weight = weight / (NUM_TAPS * NUM_TAPS);
    }

    weight = saturate(weight * 10);
    outputTex[dtid.xy] = float4(result, weight);
    outputTex[dtid.xy] = float4(result, 1.0);
}