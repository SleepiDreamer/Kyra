struct Counters
{
    uint numLights;
    float totalPower;
    uint numHeavyEntries;
    uint numLightEntries;
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