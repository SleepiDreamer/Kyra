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
}