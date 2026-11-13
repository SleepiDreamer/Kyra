import structs;
import helpers;
import settings;

Texture2D<float4> inputTexture : register(t0, space0);
RWBuffer<float> outputUav : register(u0, space0);
ConstantBuffer<PostProcessSettings> postProcessSettings : register(b0, space0);
SamplerState linearSampler : register(s0, space0);

[shader("compute")]
[numthreads(8, 8, 1)]
void FieldPass(uint3 dtid : SV_DispatchThreadID)
{
}