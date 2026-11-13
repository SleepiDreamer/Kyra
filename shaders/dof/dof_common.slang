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

// sample code from https://www.adriancourreges.com/blog/2018/12/02/ue4-optimized-post-effects/
static const float PI = 3.1415926;
static const float PI_OVER_2 = 1.5707963;
static const float PI_OVER_4 = 0.785398;
static const float EPSILON = 1e-6;

// Maps a unit square in [-1, 1] to a unit disk in [-1, 1]. Shirley 97 "A Low Distortion Map Between Disk and Square"
// Inputs: cartesian coordinates
// Return: new circle-mapped polar coordinates (radius, angle)
float2 UnitSquareToUnitDiskPolar(float a, float b) {

    float radius, angle;
    if (abs(a) > abs(b))
    {
        // First region (left and right quadrants of the disk)
        radius = a;
        angle = b / (a + EPSILON) * PI_OVER_4;
    }
    else
    {
        // Second region (top and botom quadrants of the disk)
        radius = b;
        angle = PI_OVER_2 - (a / (b + EPSILON) * PI_OVER_4);
    }
    if (radius < 0)
    {
        // Always keep radius positive
        radius *= -1.0f;
        angle += PI;
    }
    return float2(radius, angle);
}

// Maps a unit square in [-1, 1] to a unit disk in [-1, 1]
// Inputs: cartesian coordinates
// Return: new circle-mapped cartesian coordinates
float2 SquareToDiskMapping(float a, float b) {
    float2 PolarCoord = UnitSquareToUnitDiskPolar(a, b);
    return float2(PolarCoord.x * cos(PolarCoord.y), PolarCoord.x * sin(PolarCoord.y));
}

// Remap a unit square in [0, 1] to a polygon in [-1, 1] with <edgeCount> edges rotated by <shapeRotation> radians
// Inputs: cartesian coordinates
// Return: new polygon-mapped cartesian coordinates
public float2 SquareToPolygonMapping(float2 uv, float edgeCount, float shapeRotation) {
    float2 p = uv * 2.0f - 1.0f;
    float2 PolarCoord = UnitSquareToUnitDiskPolar(p.x, p.y); // (radius, angle)

    // Re-scale radius to match a polygon shape
    PolarCoord.x *=                                      cos(PI / edgeCount)
                    / //----------------------------------------------------------------------------------------------
                    cos(PolarCoord.y - (2.0f * PI / edgeCount) * floor((edgeCount*PolarCoord.y + PI) / 2.0f / PI ) );

    // Apply a rotation to the polygon shape
    PolarCoord.y +=  shapeRotation;

    return float2(PolarCoord.x * cos(PolarCoord.y), PolarCoord.x * sin(PolarCoord.y));
}