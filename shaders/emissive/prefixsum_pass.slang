import emissive_common;

struct ScanParams
{
    uint heavy; // 0:light 1:heavy
    uint numBlocks;
};

StructuredBuffer<float> powerBuffer   : register(t0, space0);
StructuredBuffer<uint> worklist       : register(t1, space0);

RWStructuredBuffer<float> prefixOut   : register(u0, space0);
RWStructuredBuffer<float> blockSums   : register(u1, space0);
RWStructuredBuffer<Counters> counters : register(u2, space0);

ConstantBuffer<ScanParams> params     : register(b0, space1);

groupshared float temp[256];


[shader("compute")]
[numthreads(256, 1, 1)]
void ScanLocal(uint3 dtid : SV_DispatchThreadID, uint gtid : SV_GroupIndex, uint3 gid : SV_GroupID)
{
    uint idx = dtid.x;
    uint count = (params.heavy != 0) ? counters[0].numHeavyEntries : counters[0].numLightEntries;

    float v = 0.0;
    if (idx < count)
    {
        v = powerBuffer[worklist[idx]];
    }

    temp[gtid] = v;
    GroupMemoryBarrierWithGroupSync();

    for (uint offset = 1; offset < 256; offset <<= 1)
    {
        float add = 0.0;
        if (gtid >= offset)
        {
            add = temp[gtid - offset];
        }
        GroupMemoryBarrierWithGroupSync();
        temp[gtid] += add;
        GroupMemoryBarrierWithGroupSync();
    }

    prefixOut[idx] = temp[gtid];

    if (gtid == 255)
    {
        blockSums[gid.x] = temp[gtid];
    }
}

[shader("compute")]
[numthreads(1, 1, 1)]
void ScanBlockSums(uint3 dtid : SV_DispatchThreadID)
{
    float running = 0.0;
    for (uint b = 0; b < params.numBlocks; b++)
    {
        float t = blockSums[b];
        blockSums[b] = running;
        running += t;
    }
}

[shader("compute")]
[numthreads(256, 1, 1)]
void ScanAddOffsets(uint3 dtid : SV_DispatchThreadID, uint3 gid : SV_GroupID)
{
    prefixOut[dtid.x] += blockSums[gid.x];
}