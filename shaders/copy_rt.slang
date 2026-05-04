import structs;

Texture2D<float4> inputTexture : register(t0, space0);
RWTexture2D<float4> outputTexture : register(u0, space0);
ConstantBuffer<RenderSettings> renderSettings : register(b0, space0);
ConstantBuffer<RenderData> renderData : register(b1, space0);
ConstantBuffer<Resolution> resolution : register(b4, space0);

struct Resolution
{
    uint width;
    uint height;
};

[shader("compute")]
[numthreads(8, 8, 1)]
void CopyRT(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= resolution.width || dtid.y >= resolution.height) return;

    uint accumulation = renderSettings.denoising ? 1 : renderData.frame + 1;
    float3 result = inputTexture[dtid.xy].rgb / accumulation;

    outputTexture[dtid.xy] = float4(result, 1.0);
}