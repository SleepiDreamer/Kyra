import structs;
import helpers;
import settings;
import rng;
import random.sampling;
import pbr;
import camera;

uniform RWTexture2D<float4> rtOutputBuffer : register(u0, space0);
uniform RWTexture2D<float4> albedoBuffer : register(u1, space0);
uniform RWTexture2D<float4> specularAlbedoBuffer : register(u2, space0);
uniform RWTexture2D<float4> normalRoughnessBuffer : register(u3, space0);
uniform RWTexture2D<float2> motionVectorsBuffer : register(u4, space0);
uniform RWTexture2D<float> depthBuffer : register(u5, space0);
uniform RaytracingAccelerationStructure sceneBVH : register(t0, space0);
StructuredBuffer<Material> materials : register(t1, space0);
SamplerState linearSampler : register(s0, space0);
ConstantBuffer<RenderSettings> renderSettings : register(b0, space0);
ConstantBuffer<RenderData> renderData : register(b1, space0);

StructuredBuffer<Vertex> vertices : register(t0, space1);
StructuredBuffer<uint> indices : register(t1, space1);
cbuffer HitGroupConstants : register(b0, space1)
{
    uint materialIndex;
};

[shader("raygeneration")]
void RayGen()
{
    uint2 idx = DispatchRaysIndex().xy;
    float2 size = DispatchRaysDimensions().xy;
    float2 uv = idx / size;

    Payload payload;
    payload.radiance = float3(0.0, 0.0, 0.0);
    payload.throughput = float3(1.0, 1.0, 1.0);
    payload.done = false;
    payload.depth = 0;
    payload.rng = RNG.create(idx, uint(size.x), renderData.frame);
    uv += float2(renderData.camera.jitterX, renderData.camera.jitterY) / size;

    Camera cam;

    RayDesc ray;
    if (DEPTH_OF_FIELD)
    {
        float3 origin;
        ray.Direction = cam.ThinLensCamera(payload.rng, uv, renderData.camera, origin);
        ray.Origin = origin;
    }
    else
    {
        ray.Direction = cam.PinholeCamera(uv, renderData.camera);
        ray.Origin = renderData.camera.position;
    }
    ray.TMin = 0.0;
    ray.TMax = MAX_RAY_DEPTH;

    while (!payload.done && payload.depth < renderSettings.bounces + 1)
    {
        TraceRay(sceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

        if (all(payload.throughput == 0.0)) payload.done = true;

        if (payload.done)
        {
            break;
        }

        // from Nvidia's Zorah talk: https://www.youtube.com/watch?v=8AnVpcIczyk&t=1107s
        if (USE_RUSSIAN_ROULETTE)
        {
            if (payload.depth >= RUSSIAN_ROULETTE_START_BOUNCE)
            {
                float luminance = pow(dot(payload.throughput, float3(0.2126, 0.7152, 0.0722)), 0.5);
                float continuationProb = min(1.0, luminance);

                uint activeLaneCount = WaveActiveCountBits(true);
                uint totalLaneCount = WaveGetLaneCount();
                float activeLaneRatio = (float)activeLaneCount / (float)totalLaneCount;
                static const float activeLaneRatioThreshold = 0.1;
                float groupContinuationProb = continuationProb * saturate(activeLaneRatio / activeLaneRatioThreshold);

                if (payload.rng.nextFloat() >= groupContinuationProb)
                {
                    payload.done = true;
                }
                else
                {
                    payload.throughput /= groupContinuationProb;
                }
            }
        }

        ray.Origin = payload.nextOrigin;
        ray.Direction = payload.nextDirection;
        payload.depth++;
    }

    float3 radiance = max(sanitize(payload.radiance), 0.0);
  
    if (renderData.frame == 0 || renderSettings.denoising)
    { 
        rtOutputBuffer[idx].rgb = radiance;
    }
    else
    {
        rtOutputBuffer[idx].rgb += radiance;
    }
}

[shader("closesthit")]
void ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint baseIdx = PrimitiveIndex() * 3;
    uint i0 = indices[baseIdx];
    uint i1 = indices[baseIdx + 1];
    uint i2 = indices[baseIdx + 2];

    Material material = materials[materialIndex];
    float2 uv = sampleTriangle(vertices[i0].uv, vertices[i1].uv, vertices[i2].uv, attr.barycentrics);

    float3 geoNormal = sampleTriangle(vertices[i0].normal, vertices[i1].normal, vertices[i2].normal, attr.barycentrics);
    geoNormal = normalize(mul(geoNormal, (float3x3)WorldToObject3x4()));
    float tangentW = vertices[i0].tangent.w * -1;
    float3 tangent = sampleTriangle(vertices[i0].tangent.xyz, vertices[i1].tangent.xyz, vertices[i2].tangent.xyz, attr.barycentrics);
    tangent = normalize(mul((float3x3)ObjectToWorld3x4(), tangent));
    float3 bitangent = cross(geoNormal, tangent) * tangentW;

    float3 normal = geoNormal;
    float3 normalMap = float3(0, 0, 1);
    if (material.normalIndex != -1)
    {
         Texture2D normalTex = DescriptorHandle<Texture2D>(uint2(material.normalIndex, 0));
         normalMap = normalTex.SampleLevel(linearSampler, uv, 0).rgb * 2 - 1;
    }
    float3x3 TBN = float3x3(tangent, bitangent, geoNormal);
    normal = normalize(mul(normalMap, TBN));
    bool backfacing = HitKind() == HIT_KIND_TRIANGLE_BACK_FACE;
    if (backfacing)
    {
        normal = -normal;
        geoNormal = -geoNormal;
    }

    float3 hitPoint = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    float4 color = sampleOrDefault(material.albedoIndex, linearSampler, uv, material.albedoFactor);
    float3 albedo = color.rgb;
    float alpha = color.a;
    if (renderSettings.whiteAlbedo) albedo = float3(1);
    float3 emission = sampleOrDefault(material.emissiveIndex, linearSampler, uv, material.emissiveFactor);
    float2 metallicRoughness = sampleOrDefault(material.metallicRoughnessIndex, linearSampler, uv, float3(1, material.roughnessFactor, material.metallicFactor)).gb;
    float roughness = max(metallicRoughness.x, 0.0001);
    float metallic = metallicRoughness.y;

    switch (renderSettings.debugMode)
    {
        case DebugMode::None: break;
        case DebugMode::Albedo:     payload.done = true; payload.radiance = LinearToSrgb(albedo); return;
        case DebugMode::Emissive:   payload.done = true; payload.radiance = LinearToSrgb(emission); return;
        case DebugMode::Metallic:   payload.done = true; payload.radiance = metallic; return;
        case DebugMode::Roughness:  payload.done = true; payload.radiance = roughness; return;
        case DebugMode::NormalMap:  payload.done = true; payload.radiance = (normalMap + 1) / 2; return;
        case DebugMode::Normal:     payload.done = true; payload.radiance = (normal + 1) / 2; return;
        case DebugMode::GeoNormal:  payload.done = true; payload.radiance = (geoNormal + 1) / 2; return;
        case DebugMode::Tangent:    payload.done = true; payload.radiance = (tangent + 1) / 2; return;
        case DebugMode::Bitangent:  payload.done = true; payload.radiance = (bitangent + 1) / 2; return;
        case DebugMode::TangentW:   payload.done = true; payload.radiance = tangentW; return;
    }

    payload.radiance += emission * payload.throughput * renderSettings.lightIntensity;

    if (alpha == 0)
    {
        float3 wo;
        payload.throughput *= PBRIndirect(payload.rng, normal, geoNormal, -WorldRayDirection(), albedo, roughness, metallic, wo);
        payload.nextDirection = wo;
        payload.nextOrigin = OffsetRay(hitPoint, geoNormal);
    }
    else
    {
        float3 wo;
        float3 offsetN;
        float eta = backfacing ? IOR_GLASS / IOR_AIR : IOR_AIR / IOR_GLASS;
        payload.throughput *= PBRTransmission(payload.rng, geoNormal, -WorldRayDirection(), albedo, eta, RayTCurrent(), wo, offsetN);
        payload.nextDirection = wo;
        payload.nextOrigin = OffsetRay(hitPoint, offsetN);
    }

    if (payload.depth == 0)
    {
        uint2 idx = DispatchRaysIndex().xy;
        float2 size = DispatchRaysDimensions().xy;

        Camera cam;
        float2 currentUV = idx / size;
        float3 unjitteredPos = renderData.camera.position + cam.PinholeCamera(currentUV, renderData.camera) * RayTCurrent();
        float2 prevUV = WorldToUV(unjitteredPos, renderData.prevCamera);
        motionVectorsBuffer[idx] = (prevUV - currentUV) * size;

        float depth = 0.5;
        albedoBuffer[idx].xyz = albedo;
        normalRoughnessBuffer[idx].xyz = normal;
        normalRoughnessBuffer[idx].w = roughness;
        depthBuffer[idx] = RayTCurrent();
        specularAlbedoBuffer[idx].xyz = EnvBRDFApprox2(lerp(0.04, albedo, metallic), roughness, saturate(dot(normal, -WorldRayDirection())));
    }
}

[shader("miss")]
void Miss(inout Payload payload)
{
    payload.done = true;

    if (renderSettings.debugMode != 0)
    {
        return;
    }

    if (renderSettings.whiteLighting == 1)
    {
        payload.radiance += float3(1) * payload.throughput;
        return;
    }

    if (renderData.hdriIndex != -1)
    {
        Texture2D hdri = DescriptorHandle<Texture2D>(uint2(renderData.hdriIndex, 0));
        float2 uv = DirectionToEquirectangular(WorldRayDirection());
        float3 env = min(hdri.SampleLevel(linearSampler, uv, 0).rgb, 30.0);
        payload.radiance += env * renderSettings.skyIntensity * payload.throughput;
    }
    else
    {
        payload.radiance += float3(0.6, 0.8, 1.0) * renderSettings.skyIntensity * payload.throughput * (WorldRayDirection().y + 1.1) / 2.1;
    }

    if (payload.depth == 0)
    {
        uint2 idx = DispatchRaysIndex().xy;
        float2 size = DispatchRaysDimensions().xy;

        albedoBuffer[idx] = float4(0.0, 0.0, 0.0, 1.0);
        specularAlbedoBuffer[idx] = float4(0.5, 0.5, 0.5, 1.0);
        normalRoughnessBuffer[idx] = float4(0.0, 0.0, 0.0, 1.0);
        depthBuffer[idx] = MAX_RAY_DEPTH;

        Camera cam;
        float2 currentUV = idx / size;
        float3 unjitteredPos = renderData.camera.position + cam.PinholeCamera(currentUV, renderData.camera) * MAX_RAY_DEPTH;
        float2 prevUV = WorldToUV(unjitteredPos, renderData.prevCamera);
        motionVectorsBuffer[idx] = (prevUV - currentUV) * size;
    }
}

[shader("anyhit")]
void AnyHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    Material material = materials[materialIndex];
    uint baseIdx = PrimitiveIndex() * 3;
    uint i0 = indices[baseIdx];
    uint i1 = indices[baseIdx + 1];
    uint i2 = indices[baseIdx + 2];
    float2 uv = sampleTriangle(vertices[i0].uv, vertices[i1].uv, vertices[i2].uv, attr.barycentrics);
    float alpha = sampleOrDefault(material.albedoIndex, linearSampler, uv, float4(1)).a;

    if (alpha > 0.5f)
    {
        return;
    }
    IgnoreHit();
}