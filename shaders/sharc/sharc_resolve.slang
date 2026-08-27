import structs;
import settings;

#define SHARC_UPDATE 1
#define SHARC_ENABLE_64_BIT_ATOMICS 1
#define SHARC_ENABLE_GLSL 0
#include "SharcCommon.h"

ConstantBuffer<RenderData> renderData : register(b0, space0);

RWStructuredBuffer<uint64_t>                u_SharcHashEntriesBuffer    : register(u0, space0);
RWStructuredBuffer<SharcAccumulationData>   u_SharcAccumulationBuffer   : register(u1, space0);
RWStructuredBuffer<SharcPackedData>         u_SharcResolvedBuffer       : register(u2, space0);

[shader("compute")]
[numthreads(256, 1, 1)]
void SharcResolve(in uint2 did : SV_DispatchThreadID)
{
    SharcParameters sharcParameters;

    sharcParameters.gridParameters.cameraPosition = renderData.camera.position;
    sharcParameters.gridParameters.sceneScale = SHARC_SCENE_SCALE;
    sharcParameters.gridParameters.logarithmBase = SHARC_GRID_LOGARITHM_BASE;
    sharcParameters.gridParameters.levelBias = SHARC_GRID_LEVEL_BIAS;

    sharcParameters.hashMapData.capacity = SHARC_CAPACITY;
    sharcParameters.hashMapData.hashEntriesBuffer = u_SharcHashEntriesBuffer;

    sharcParameters.accumulationBuffer = u_SharcAccumulationBuffer;
    sharcParameters.resolvedBuffer = u_SharcResolvedBuffer;
    sharcParameters.radianceScale = SHARC_RADIANCE_SCALE;

    SharcResolveParameters resolveParameters;
    resolveParameters.accumulationFrameNum = SHARC_ACCUMULATION_FRAMES;
    resolveParameters.staleFrameNumMax = SHARC_STALE_FRAMES;
    resolveParameters.cameraPositionPrev = renderData.prevCamera.position;
    resolveParameters.frameIndex = renderData.frame;

    SharcResolveEntry(did.x, sharcParameters, resolveParameters);
}