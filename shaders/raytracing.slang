import structs;
import helpers;
import settings;
import rng;
import random.sampling;
import emissive.lights;
import emissive.emissive_common;
import pbr;
import camera;

#include "sharc/SharcCommon.h"

RWTexture2D<float4> rtOutputBuffer            : register(u0, space0);
RWTexture2D<float4> albedoBuffer              : register(u1, space0);
RWTexture2D<float4> specularAlbedoBuffer      : register(u2, space0);
RWTexture2D<float4> normalRoughnessBuffer     : register(u3, space0);
RWTexture2D<float2> motionVectorsBuffer       : register(u4, space0);
RWTexture2D<float> depthBuffer                : register(u5, space0);

RaytracingAccelerationStructure sceneBVH      : register(t0, space0);
StructuredBuffer<Material> materials          : register(t1, space0);
StructuredBuffer<Light> lights                : register(t2, space0);
StructuredBuffer<float> powerBuffer           : register(t3, space0);
StructuredBuffer<AliasRow> aliasTable         : register(t4, space0);

SamplerState linearSampler                    : register(s0, space0);

ConstantBuffer<RenderSettings> renderSettings : register(b0, space0);
ConstantBuffer<RenderData> renderData         : register(b1, space0);

RWStructuredBuffer<uint64_t> sharcHashEntries               : register(u0, space6);
RWStructuredBuffer<SharcAccumulationData> sharcAccumulation  : register(u1, space6);
RWStructuredBuffer<SharcPackedData> sharcResolved            : register(u2, space6);

StructuredBuffer<Vertex> vertices             : register(t0, space1);
StructuredBuffer<uint> indices                : register(t1, space1);
StructuredBuffer<float> powers                : register(t2, space1);
cbuffer HitGroupConstants                     : register(b0, space1)
{
    uint materialIndex;
};

SharcParameters GetSharcParameters()
{
    SharcParameters sharcParameters;

    sharcParameters.gridParameters.cameraPosition = renderData.camera.position;
    sharcParameters.gridParameters.sceneScale = SHARC_SCENE_SCALE;
    sharcParameters.gridParameters.logarithmBase = SHARC_GRID_LOGARITHM_BASE;
    sharcParameters.gridParameters.levelBias = SHARC_GRID_LEVEL_BIAS;

    sharcParameters.hashMapData.capacity = SHARC_CAPACITY;
    sharcParameters.hashMapData.hashEntriesBuffer = sharcHashEntries;

    sharcParameters.accumulationBuffer = sharcAccumulation;
    sharcParameters.resolvedBuffer = sharcResolved;
    sharcParameters.radianceScale = SHARC_RADIANCE_SCALE;
    sharcParameters.enableAntiFireflyFilter = true;

    return sharcParameters;
}

float GetSharcVoxelSize(float3 positionWorld)
{
    HashGridParameters gridParameters = GetSharcParameters().gridParameters;
    return HashGridGetVoxelSize(HashGridGetLevel(positionWorld, gridParameters), gridParameters);
}

[shader("raygeneration")]
void RayGen()
{
    uint2 idx = DispatchRaysIndex().xy;

#if SHARC_UPDATE
    if (idx.x % SHARC_UPDATE_STRIDE != 0 || idx.y % SHARC_UPDATE_STRIDE != 0)
    {
        return;
    }
#endif

    float2 size = DispatchRaysDimensions().xy;
#if SHARC_UPDATE
    RNG rng = RNG.create(idx, uint(size.x), renderData.frame + SHARC_UPDATE_RNG_OFFSET);
    uint2 tileJitter = min(uint2(rng.nextFloat2() * float(SHARC_UPDATE_STRIDE)), uint2(SHARC_UPDATE_STRIDE - 1));
    idx = min(idx + tileJitter, uint2(size) - 1);
#else
    RNG rng = RNG.create(idx, uint(size.x), renderData.frame);
#endif
    float2 uv = idx / size;
    
    Payload payload;
    payload.radiance = float3(0.0, 0.0, 0.0);
    payload.throughput = float3(1.0, 1.0, 1.0);
    payload.done = false;
    payload.depth = 0;
    payload.bsdfPdf = -1.0;
    payload.rng = rng;
    payload.coneWidth = 0.0;
    payload.coneSpreadAngle = 2.0 * tan(radians(renderData.camera.fov) * 0.5) / size.y;
    uv += float2(renderData.camera.jitterX, renderData.camera.jitterY) / size;

#if SHARC_UPDATE
    SharcParameters sharcParameters = GetSharcParameters();
    SharcState sharcState;
    SharcInit(sharcState);
#endif

    RayDesc ray;
    Camera cam;

    if (DEPTH_OF_FIELD)
    {
        float3 origin;
        ray.Direction = cam.AnamorphicCamera(payload.rng, uv, renderData.camera, origin);
        if (length(ray.Direction) < 0.5) return;
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
#if SHARC_UPDATE
        payload.sharcRadiance = float3(0.0, 0.0, 0.0);
        payload.sharcWeight = float3(0.0, 0.0, 0.0);
        payload.sharcCacheable = false;
#endif

        if (SER && SHARC_UPDATE)
        {
            HitObject hitObj = HitObject.TraceRay<Payload>(sceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

            uint hint = hitObj.IsHit();
            ReorderThread(hitObj, hint, 1);

            hitObj.Invoke<Payload>(hitObj, payload);
        }
        else
        {
            TraceRay(sceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
        }

#if SHARC_UPDATE
        if (payload.sharcCacheable)
        {
            SharcHitData sharcHitData;
            sharcHitData.positionWorld = payload.nextOrigin;
            sharcHitData.normalWorld = payload.nextOffsetNormal;

            if (!SharcUpdateHit(sharcParameters, sharcState, sharcHitData, payload.sharcRadiance, payload.rng.nextFloat()))
            {
                payload.done = true;
            }
        }
        else
        {
            SharcUpdateMiss(sharcParameters, sharcState, payload.sharcRadiance);
        }

        if (!payload.done)
        {
            SharcSetThroughput(sharcState, payload.sharcWeight);
        }
#endif

        if (all(payload.throughput == 0.0)) 
        { 
            payload.done = true;
        }

        if (payload.done)
        {
            break;
        }

        // from Nvidia's Zorah talk: https://www.youtube.com/watch?v=8AnVpcIczyk&t=1107s
        if (USE_RUSSIAN_ROULETTE && !renderSettings.sharc)
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
#if SHARC_UPDATE
                    SharcSetThroughput(sharcState, float3(1.0 / groupContinuationProb));
#endif
                }
            }
        }

        ray.Origin = OffsetRay(payload.nextOrigin, payload.nextOffsetNormal);
        ray.Direction = payload.nextDirection;
        payload.depth++;
    }

#if !SHARC_UPDATE
    float3 radiance = max(sanitize(payload.radiance), 0.0);

    if (renderData.frame == 0 || renderSettings.denoising)
    {
        rtOutputBuffer[idx].rgb = radiance;
    }
    else
    {
        rtOutputBuffer[idx].rgb += radiance;
    }
#endif
}

[shader("closesthit")]
void ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint2 idx = DispatchRaysIndex().xy;
    float2 size = DispatchRaysDimensions().xy;
    float2 screenUv = idx / size;
    bool left = screenUv.x < renderSettings.compSlider;
    bool right = !left;

    uint baseIdx = PrimitiveIndex() * 3;
    uint i0 = indices[baseIdx];
    uint i1 = indices[baseIdx + 1];
    uint i2 = indices[baseIdx + 2];

    Material material = materials[materialIndex];
    SamplerState sampler = DescriptorHandle<SamplerState>(uint2(material.samplerIndex, 0));
    float2 uv = sampleTriangle(vertices[i0].uv, vertices[i1].uv, vertices[i2].uv, attr.barycentrics);
    if (material.flags & MAT_FLAG_UV_TRANSFORM) uv = mul(float3(uv, 1.0), material.uvTransform);

    float3 flatNormal = cross(vertices[i1].position - vertices[i0].position, vertices[i2].position - vertices[i0].position);
    flatNormal = normalize(mul(flatNormal, (float3x3)WorldToObject3x4()));
    float3 smoothNormal = sampleTriangle(vertices[i0].normal, vertices[i1].normal, vertices[i2].normal, attr.barycentrics);
    smoothNormal = normalize(mul(smoothNormal, (float3x3)WorldToObject3x4()));
    float tangentW = vertices[i0].tangent.w * -1;
    float3 tangent = sampleTriangle(vertices[i0].tangent.xyz, vertices[i1].tangent.xyz, vertices[i2].tangent.xyz, attr.barycentrics);
    tangent = normalize(mul((float3x3)ObjectToWorld3x4(), tangent));
    float3 bitangent = cross(smoothNormal, tangent) * tangentW;

    float3 normal = smoothNormal;
    float3 normalMap = float3(0, 0, 1);
    if (material.normalIndex != -1)
    {
         Texture2D normalTex = DescriptorHandle<Texture2D>(uint2(material.normalIndex, 0));
         normalMap = normalTex.SampleLevel(sampler, uv, 0).rgb * 2 - 1;
    }
    float3x3 TBN = float3x3(tangent, bitangent, smoothNormal);
    normal = normalize(mul(normalMap, TBN));
    bool backfacing = HitKind() == HIT_KIND_TRIANGLE_BACK_FACE;
    if (backfacing)
    {
        normal = -normal;
        smoothNormal = -smoothNormal;
        flatNormal = -flatNormal;
    }

    float3 hitPoint = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    float4 color = sampleOrDefault(material.albedoIndex, sampler, uv, material.albedoFactor);
    float3 albedo = Rec709ToACEScg(color.rgb);
    float alpha = color.a;
    if (renderSettings.whiteAlbedo) albedo = float3(1);
    float3 emission = Rec709ToACEScg(sampleOrDefault(material.emissiveIndex, sampler, uv, material.emissiveFactor));
    float2 metallicRoughness = sampleOrDefault(material.metallicRoughnessIndex, sampler, uv, 
                                               float3(1, material.roughnessFactor, material.metallicFactor)).gb;
    float roughness = max(metallicRoughness.x * metallicRoughness.x, 0.00025);
    float metallic = metallicRoughness.y;
    bool isTransmission = material.flags & MAT_FLAG_TRANSMISSION;

    bool sharcCacheable = !isTransmission && roughness > SHARC_MIN_ROUGHNESS;

    float coneWidth = payload.coneWidth + payload.coneSpreadAngle * RayTCurrent();

#if !SHARC_UPDATE
    switch (renderSettings.debugMode)
    {
        case DebugMode::None: break;
        case DebugMode::Albedo:     payload.done = true; payload.radiance = LinearToSrgb(albedo); return;
        case DebugMode::Emissive:   payload.done = true; payload.radiance = LinearToSrgb(emission); return;
        case DebugMode::Metallic:   payload.done = true; payload.radiance = metallic; return;
        case DebugMode::Roughness:  payload.done = true; payload.radiance = roughness; return;
        case DebugMode::NormalMap:  payload.done = true; payload.radiance = (normalMap + 1) / 2; return;
        case DebugMode::Normal:     payload.done = true; payload.radiance = (normal + 1) / 2; return;
        case DebugMode::GeoNormal:  payload.done = true; payload.radiance = (smoothNormal + 1) / 2; return;
        case DebugMode::Tangent:    payload.done = true; payload.radiance = (tangent + 1) / 2; return;
        case DebugMode::Bitangent:  payload.done = true; payload.radiance = (bitangent + 1) / 2; return;
        case DebugMode::TangentW:   payload.done = true; payload.radiance = tangentW; return;
        case DebugMode::SharcRadiance:
        {
            payload.done = true;
            SharcHitData sharcHitData;
            sharcHitData.positionWorld = hitPoint;
            sharcHitData.normalWorld = flatNormal;
            float3 cachedRadiance = float3(0.0, 0.0, 0.0);
            SharcGetCachedRadiance(GetSharcParameters(), sharcHitData, cachedRadiance, true);
            payload.radiance = cachedRadiance;
            return;
        }
        case DebugMode::SharcOccupancy:
        {
            payload.done = true;
            SharcHitData sharcHitData;
            sharcHitData.positionWorld = hitPoint;
            sharcHitData.normalWorld = flatNormal;
            float3 cachedRadiance = float3(0.0, 0.0, 0.0);
            bool cacheHit = SharcGetCachedRadiance(GetSharcParameters(), sharcHitData, cachedRadiance, false);
            // green: query hits the cache, red: eligible but no entry yet
            // blue: excluded as transmissive, yellow: excluded as too smooth
            if (isTransmission)            payload.radiance = float3(0.0, 0.0, 1.0);
            else if (!sharcCacheable)      payload.radiance = float3(1.0, 1.0, 0.0);
            else                           payload.radiance = cacheHit ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
            return;
        }
        case DebugMode::SharcGrid:
            payload.done = true;
            payload.radiance = HashGridDebugColoredHash(hitPoint, flatNormal, GetSharcParameters().gridParameters);
            return;
    }
#endif

#if SHARC_QUERY
    if (renderSettings.sharc && sharcCacheable && payload.depth >= SHARC_QUERY_MIN_DEPTH &&
        coneWidth > GetSharcVoxelSize(hitPoint) * SHARC_MIN_SPREAD_VOXELS)
    {
        SharcHitData sharcHitData;
        sharcHitData.positionWorld = hitPoint;
        sharcHitData.normalWorld = flatNormal;

        float3 cachedRadiance;
        if (SharcGetCachedRadiance(GetSharcParameters(), sharcHitData, cachedRadiance, false))
        {
            payload.radiance += cachedRadiance * payload.throughput;
            payload.done = true;
            return;
        }
    }
#endif

    float3 localRadiance = float3(0.0, 0.0, 0.0);

    if (any(emission > 0.0))
    {
        bool prevNEEActive = payload.bsdfPdf >= 0.0 && renderData.numLights > 0 && renderData.totalPower > 0.0;
        if (prevNEEActive)
        {
            // add emission with MIS weight
            float cosTheta_light = abs(dot(flatNormal, -WorldRayDirection()));
            float t = RayTCurrent();
            float p_light = Grayscale(emission) * t * t / (renderData.totalPower * max(cosTheta_light, 0.001));
            float w_bsdf = payload.bsdfPdf / max(payload.bsdfPdf + p_light, 1e-7);
            localRadiance += emission * renderSettings.lightIntensity * w_bsdf;
        }
        else
        {
            // previous bounce didn't have NEE, add emission at full weight
            localRadiance += emission * renderSettings.lightIntensity;
        }
    }

    // explicit lighting
    if (renderData.numLights > 0 && renderSettings.lightIntensity > 0.0 && !isTransmission)
    {
        float2 rand = payload.rng.nextFloat2();
        float wn = renderData.totalPower / renderData.numLights;
        uint i = min(uint(floor(rand.x * renderData.numLights)), renderData.numLights - 1);
        uint lightIdx = (rand.y * wn < aliasTable[i].threshold) ? i : aliasTable[i].alias;
        float pdf = powerBuffer[lightIdx] / renderData.totalPower;

        Light light = lights[lightIdx];
        uint lightType = light.GetType();
        float3 radiance;
        float3 dirToLight;
        float dist;

        if (lightType == LIGHT_TYPE_DIRECTIONAL)
        {
            radiance = light.SampleDirectionalLight(payload.rng.nextFloat2(), dist, dirToLight);
        }
        else if (lightType == LIGHT_TYPE_POINT)
        {
            radiance = light.SamplePointLight(payload.rng.nextFloat2(), hitPoint, dist, dirToLight);
        }
        else if (lightType == LIGHT_TYPE_TRIANGLE)
        {
            Material triMaterial = materials[light.TriMatIdx()];
            radiance = light.SampleTriangleLight(payload.rng.nextFloat2(), hitPoint, dist, dirToLight, triMaterial);
        }

        if (dot(dirToLight, normal) > 0.0)
        {
            RayDesc shadowRay;
            shadowRay.Origin = OffsetRay(hitPoint, flatNormal);
            shadowRay.Direction = dirToLight;
            shadowRay.TMin = 0.0;
            shadowRay.TMax = dist - 0.001;

            ShadowPayload shadowPayload;
            shadowPayload.inShadow = true;

            TraceRay(sceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                     0xFF, 0, 1, 1, shadowRay, shadowPayload);

            if (!shadowPayload.inShadow)
            {
                float3 outgoing = radiance * PBRDirect(normal, dirToLight, -WorldRayDirection(), albedo, roughness, metallic);

                float w_nee = 1.0;
                if (lightType == LIGHT_TYPE_TRIANGLE)
                {
                    float triArea          = 0.5 * length(cross(light.TriE1(), light.TriE2()));
                    float cosTheta_light   = abs(dot(light.TriNormal(), -dirToLight));
                    // Convert light selection probability to solid-angle PDF
                    float p_light_solidangle = pdf * dist * dist / (triArea * max(cosTheta_light, 0.001));
                    float p_bsdf = PBRIndirectPdf(normal, -WorldRayDirection(), dirToLight, roughness, metallic, albedo);
                    w_nee = p_light_solidangle / max(p_light_solidangle + p_bsdf, 1e-7);
                }
                localRadiance += outgoing * renderSettings.lightIntensity / pdf * w_nee;
            }
        }
    }

    payload.radiance += localRadiance * payload.throughput;

    payload.coneWidth = coneWidth;

    float3 bsdfWeight;
    if (!isTransmission)
    {
        // Opaque
        float3 wo;
        bool isSpecularLobe;
        bsdfWeight = PBRIndirect(payload.rng, normal, smoothNormal, -WorldRayDirection(), albedo, roughness, metallic, wo, isSpecularLobe);
        payload.throughput *= bsdfWeight;
        payload.nextDirection = wo;
        payload.nextOrigin = hitPoint;
        payload.nextOffsetNormal = flatNormal;
        payload.bsdfPdf = PBRIndirectPdf(normal, -WorldRayDirection(), wo, roughness, metallic, albedo);
        payload.coneSpreadAngle += isSpecularLobe ? 2.0 * atan(roughness) : SHARC_DIFFUSE_SPREAD_ANGLE;
    }
    else
    {
        // Transmission
        float3 wo;
        float3 offsetN;
        float eta = backfacing ? material.ior / 1.0 : 1.0 / material.ior;
        bsdfWeight = PBRTransmission(payload.rng, normal, flatNormal, -WorldRayDirection(), albedo, eta, RayTCurrent(), wo, offsetN);
        payload.throughput *= bsdfWeight;
        payload.nextDirection = wo;
        payload.nextOrigin = hitPoint;
        payload.nextOffsetNormal = offsetN;
        payload.bsdfPdf = -1.0;
    }

#if SHARC_UPDATE
    payload.sharcRadiance = localRadiance;
    payload.sharcWeight = bsdfWeight;
    payload.sharcCacheable = sharcCacheable;
#endif

#if !SHARC_UPDATE
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
#endif
}

[shader("miss")]
void Miss(inout Payload payload)
{
    payload.done = true;

#if !SHARC_UPDATE
    if (renderSettings.debugMode != 0)
    {
        return;
    }
#endif

    if (renderSettings.whiteLighting == 1)
    {
        payload.radiance += float3(1) * payload.throughput;
#if SHARC_UPDATE
        payload.sharcRadiance = float3(1);
#endif
        return;
    }

#if !SHARC_UPDATE
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
#endif

    if (renderSettings.skyIntensity == 0.0)
    {
        return;
    }

    float3 skyRadiance;
    if (renderData.hdriIndex != -1)
    {
        Texture2D hdri = DescriptorHandle<Texture2D>(uint2(renderData.hdriIndex, 0));
        float2 uv = DirectionToEquirectangular(WorldRayDirection());
        float3 env = Rec709ToACEScg(min(hdri.SampleLevel(linearSampler, uv, 0).rgb, 30.0));
        skyRadiance = env * renderSettings.skyIntensity;
    }
    else
    {
        skyRadiance = float3(0.6, 0.8, 1.0) * renderSettings.skyIntensity * (WorldRayDirection().y + 1.1) / 2.1;
    }

    payload.radiance += skyRadiance * payload.throughput;

#if SHARC_UPDATE
    payload.sharcRadiance = skyRadiance;
#endif
}

[shader("anyhit")]
void AnyHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    Material material = materials[materialIndex];
    SamplerState sampler = DescriptorHandle<SamplerState>(uint2(material.samplerIndex, 0));
    uint baseIdx = PrimitiveIndex() * 3;
    uint i0 = indices[baseIdx];
    uint i1 = indices[baseIdx + 1];
    uint i2 = indices[baseIdx + 2];
    float2 uv = sampleTriangle(vertices[i0].uv, vertices[i1].uv, vertices[i2].uv, attr.barycentrics);
    float alpha = sampleOrDefault(material.albedoIndex, sampler, uv, float4(1)).a;

    if (alpha > 0.5f)
    {
        return;
    }
    IgnoreHit();
}

[shader("miss")]
void ShadowMiss(inout ShadowPayload payload)
{
    payload.inShadow = false;
}