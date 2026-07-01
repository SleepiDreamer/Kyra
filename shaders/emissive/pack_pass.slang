import emissive_common;

struct PackParams { uint numSplits; uint numActive; };

StructuredBuffer<float> powerBuffer : register(t0, space0);
StructuredBuffer<uint>  light       : register(t1, space0);
StructuredBuffer<uint>  heavy       : register(t2, space0);
StructuredBuffer<SplitEntry> splits : register(t3, space0);

RWStructuredBuffer<AliasRow> aliasTable : register(u0, space0);
RWStructuredBuffer<Counters> counters : register(u1, space0);

ConstantBuffer<PackParams> p            : register(b0, space1);


[shader("compute")]
[numthreads(64, 1, 1)]
void Pack(uint3 dtid : SV_DispatchThreadID)
{
    uint sec = dtid.x;                 // section 0 .. s-1  (pack call k = sec+1)
    uint s = p.numSplits;
    if (sec >= s) return;

    uint N  = p.numActive;
    uint nH = counters[0].numHeavyEntries;
    float WN = counters[0].totalPower / (float)N;

    uint nPrevBuckets = (uint)(((uint64_t)N * sec)       / s);
    uint nCurBuckets  = (uint)(((uint64_t)N * (sec + 1)) / s);

    uint jPrev = splits[sec].j;
    uint jCur  = splits[sec + 1].j;
    float spill = splits[sec].spill;

    uint iPrev = nPrevBuckets - jPrev;
    uint iCur  = nCurBuckets  - jCur;

    // Start state: straddling heavy item is heavy[jPrev], with 'spill' weight left.
    uint i = iPrev;
    uint j = jPrev;
    float w = spill;
    if (spill == 0.0)
    {
        if (jPrev < nH)
        {
            w = powerBuffer[heavy[jPrev]];
        }
        else
        {
            w = WN + 1.0;
        }
    }

    [loop]
    while (true)
    {
        if (w <= WN)
        {
            // Pack heavy bucket: heavy[j] gets residual w, aliased to the next heavy.
            if (j >= jCur) break;
            uint hItem = heavy[j];
            uint hNext = (j + 1 < nH) ? heavy[j + 1] : hItem; // sentinel: self-alias at the end
            aliasTable[hItem].threshold = w;                  // raw weight; sampler scales by 1/WN
            aliasTable[hItem].alias     = hNext;
            w = (w + ((j + 1 < nH) ? powerBuffer[hNext] : WN)) - WN;
            j++;
        }
        else
        {
            // Pack light bucket: light[i] keeps its own weight, aliased to current heavy.
            if (i >= iCur) break;
            uint lItem = light[i];
            aliasTable[lItem].threshold = powerBuffer[lItem];
            aliasTable[lItem].alias     = heavy[j];
            w = (w + powerBuffer[lItem]) - WN;
            i++;
        }
    }
}