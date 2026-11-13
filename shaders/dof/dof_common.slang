module dof_common;

public cbuffer DescriptorIndices : register(b4, space0)
{
    public uint colorIdx;
    public uint depthIdx;
    public uint nearIdx;
    public uint farIdx;
    public uint outputIdx;
};

public float CalculateCircleOfConfusion(float focusDist, float sampleDist, float aperture)
{
    float coc = aperture * ((sampleDist - focusDist) / sampleDist);
    return coc;
}