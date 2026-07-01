import emissive_common;

struct SplitParams { uint numSplits; uint numActive; };   // s, N

StructuredBuffer<float> lightPrefix : register(t0, space0);
StructuredBuffer<float> heavyPrefix : register(t1, space0);

RWStructuredBuffer<SplitEntry> splits : register(u0, space0);
RWStructuredBuffer<Counters> counters : register(u1, space0);

ConstantBuffer<SplitParams> p         : register(b0, space1);

// "sum of the first c items" from an inclusive 0-indexed prefix array.
float lightSum(uint c) { return c == 0 ? 0.0 : lightPrefix[c - 1]; }
float heavySum(uint c) { return c == 0 ? 0.0 : heavyPrefix[c - 1]; }

[shader("compute")]
[numthreads(64, 1, 1)]
void Split(uint3 dtid : SV_DispatchThreadID)
{
    uint k = dtid.x;
    uint s = p.numSplits;
    if (k > s) return;

    uint nL = counters[0].numLightEntries;
    uint nH = counters[0].numHeavyEntries;
    uint N  = p.numActive;                 // == nL + nH
    float WN = counters[0].totalPower / (float)N;

    // Boundary sentinels: left edge and right edge of the whole table.
    if (k == 0) { splits[0].j = 0;  splits[0].spill = 0.0; return; }
    if (k == s) { splits[s].j = nH; splits[s].spill = 0.0; return; } // i = N - nH = nL

    uint nPrime = (uint)(((uint64_t)N * k) / s);   // buckets to the left of this boundary
    float T = (float)nPrime * WN;

    uint lo = (nPrime > nL) ? (nPrime - nL) : 0u;
    uint hi = min(nPrime, nH);
    while (lo < hi)
    {
        uint mid = (lo + hi + 1) >> 1;     // round up: we move lo up to mid
        uint jj = mid;
        uint ii = nPrime - jj;
        float sigma = lightSum(ii) + heavySum(jj);
        if (sigma <= T) lo = mid; else hi = mid - 1;
    }

    uint j = lo;
    uint i = nPrime - j;
    float sigma   = lightSum(i) + heavySum(j);
    float wStrad  = heavySum(j + 1) - heavySum(j);   // weight of the straddling heavy item h[j]
    splits[k].j     = j;
    splits[k].spill = wStrad + sigma - T;            // leftover of h[j] spilled to the right
}