module random;

interface IRandom
{
    [mutating] uint nextUint();

    [mutating] float nextFloat();

    [mutating] float2 nextFloat2();

    [mutating] float3 nextFloat3();
}

extension<T : IRandom> T
{
    [mutating] float nextFloat()
    {
        return nextUint() * (1.0f / 4294967296.0f);
    }

    [mutating] float2 nextFloat2()
    {
        return float2(nextFloat(), nextFloat());
    }

    [mutating] float3 nextFloat3()
    {
        return float3(nextFloat(), nextFloat(), nextFloat());
    }
}