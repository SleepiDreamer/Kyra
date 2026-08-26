struct Counters
{
    uint numLights;
    float totalPower;
    uint numLightEntries;
    uint numHeavyEntries;
}

struct SplitEntry
{
    uint j;
    float spill;
}

struct AliasRow
{
    float threshold;
    uint alias;
}