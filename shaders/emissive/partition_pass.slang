import emissive_common;

struct Data
{
    uint _pad0;
};

StructuredBuffer<float> powerBuffer   : register(t0, space0);

RWStructuredBuffer<Counters> counters : register(u0, space0);
RWStructuredBuffer<uint> light        : register(u1, space0);
RWStructuredBuffer<uint> heavy        : register(u2, space0);

ConstantBuffer<Data> data             : register(b0, space1);

SamplerState pointSampler             : register(s0, space0);

[shader("compute")]
[numthreads(32, 1, 1)]
void Partition(uint3 dtid: SV_DispatchThreadID)
{
    uint i = dtid.x;
    bool inRange = (i < counters[0].numLights);

    float wn = counters[0].totalPower / counters[0].numLights;
    bool isHeavy = inRange && (powerBuffer[i] > wn);
    bool isLight = inRange && !isHeavy;

    uint heavyOffset = WavePrefixCountBits(isHeavy);
    uint lightOffset = WavePrefixCountBits(isLight);

    uint heavyTotal = WaveActiveCountBits(isHeavy);
    uint lightTotal = WaveActiveCountBits(isLight);

    uint heavyBase = 0;
    uint lightBase = 0;
    if (WaveIsFirstLane())
    {
        if (heavyTotal > 0) 
        { 
            InterlockedAdd(counters[0].numHeavyEntries, heavyTotal, heavyBase);
        }
        if (lightTotal > 0) 
        { 
            InterlockedAdd(counters[0].numLightEntries, lightTotal, lightBase);
        }
    }
    heavyBase = WaveReadLaneFirst(heavyBase);
    lightBase = WaveReadLaneFirst(lightBase);

    if (isHeavy) 
    { 
        heavy[heavyBase + heavyOffset] = i;
    }
    if (isLight) 
    {
        light[lightBase + lightOffset] = i;
    }
}