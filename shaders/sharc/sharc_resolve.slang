import structs;

#define SHARC_UPDATE 1
#define SHARC_ENABLE_64_BIT_ATOMICS 1
#define SHARC_ENABLE_GLSL 0
#include "SharcCommon.h"

ConstantBuffer<RenderData> renderData : register(b0, space0);

RWStructuredBuffer<uint64_t>                u_SharcHashEntriesBuffer    : register(u0, space0);
RWStructuredBuffer<SharcAccumulationData>   u_SharcAccumulationBuffer   : register(u1, space0);
RWStructuredBuffer<SharcPackedData>         u_SharcResolvedBuffer       : register(u2, space0);

[numthreads(256, 1, 1)]
void SharcResolve(in uint2 did : SV_DispatchThreadID)
{
    SharcParameters sharcParameters;

    sharcParameters.gridParameters.cameraPosition = renderData.camera.position;
    sharcParameters.gridParameters.sceneScale = 1.0;
    sharcParameters.gridParameters.logarithmBase = SHARC_GRID_LOGARITHM_BASE;
    sharcParameters.gridParameters.levelBias = SHARC_GRID_LEVEL_BIAS;

    sharcParameters.hashMapData.capacity = 1 << 22;
    sharcParameters.hashMapData.hashEntriesBuffer = u_SharcHashEntriesBuffer;

    sharcParameters.accumulationBuffer = u_SharcAccumulationBuffer;
    sharcParameters.resolvedBuffer = u_SharcResolvedBuffer;
    sharcParameters.radianceScale = 1e3f;

    SharcResolveParameters resolveParameters;
    resolveParameters.accumulationFrameNum = 32;
    resolveParameters.staleFrameNumMax = 64;
    resolveParameters.cameraPositionPrev = renderData.prevCamera.position;
    resolveParameters.frameIndex = renderData.frame;

    SharcResolveEntry(did.x, sharcParameters, resolveParameters);
}