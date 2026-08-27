module structs;
import rng;

public [raypayload] struct Payload
{
    public float3 radiance : write(caller, closesthit, miss) : read(caller, closesthit, miss);
    public float depth : write(caller) : read(caller, closesthit, miss);
    public float3 throughput : write(caller, closesthit) : read(caller, closesthit, miss);
    public bool done : write(caller, closesthit, miss) : read(caller, closesthit, miss);
    public float3 nextOrigin : write(caller, closesthit) : read(caller);
    public float3 nextDirection : write(caller, closesthit) : read(caller);
    public RNG rng : write(caller, closesthit) : read(caller, closesthit);
    public float bsdfPdf : write(caller, closesthit) : read(caller, closesthit);
    // SHaRC, only meaningful in the SHARC_UPDATE variant of raytracing.slang. The cache state itself lives in
    // RayGen (a SharcState is far too large for a payload), so the hit shaders only report what an update needs.
    // The hit position is not passed along: nextOrigin is the same point, moved by a tiny fraction of a voxel.
    //
    // Keep this struct 112 bytes (28 floats). At exactly 124 bytes (31 floats) the driver tested here
    // (NVIDIA 32.0.16.1088, RTX 4070 SUPER) hangs the device inside CreateStateObject; 120 and 128 bytes are
    // fine, so it is a size-specific driver bug, not a payload budget. MaxPayloadSizeInBytes in RTPipeline.cpp
    // has to match whatever this adds up to.
    public float3 sharcRadiance : write(caller, closesthit, miss) : read(caller);   // radiance emitted at this vertex, without the path throughput
    public float3 sharcWeight : write(caller, closesthit) : read(caller);           // BSDF weight of the bounce leaving this vertex
    public float3 sharcNormal : write(closesthit) : read(caller);                   // geometric normal, for the hash grid key
    public bool sharcCacheable : write(caller, closesthit) : read(caller);          // false for a miss or a vertex that must stay out of the cache
};

public [raypayload] struct ShadowPayload
{
    public bool inShadow : write(caller, miss) : read(caller);
};

public struct CameraData
{
    public float3 position;
    public float fov;
    public float3 forward;
    public float jitterX;
    public float3 right;
    public float jitterY;
    public float3 up;
    public float aperture;
    public float focusDistance;
    public bool autoFocus;
    public float squeezeFactor;
    public float swirliness;
};

public struct RenderData
{
    public CameraData camera;
    public CameraData prevCamera;
    public int hdriIndex;
    public uint frame;
    public uint numLights;
    public float totalPower;
    public float deltaTime;
    public bool hdrEnabled;
}

public enum DebugMode
{
    None,
    Albedo,
    Emissive,
    Metallic,
    Roughness,
    NormalMap,
    Normal,
    GeoNormal,
    Tangent,
    Bitangent,
    TangentW,
    SharcRadiance,      // radiance the cache holds at the first hit
    SharcOccupancy,     // green where a query would hit the cache, red where it would miss
    SharcGrid,          // hash grid voxel/level visualisation
};

enum DLSSQuality
{
    UltraPerformance = 0,
    Performance = 1,
    Balanced = 2,
    Quality = 3,
    DLAA = 4,
};

public struct RenderSettings
{
    float frameLimit;
    public DebugMode debugMode;
    public uint bounces;
    public float compSlider;
    public float skyIntensity;
    public float lightIntensity;
    public bool whiteAlbedo;
    public bool whiteLighting;
    public bool denoising;
    DLSSQuality dlssQuality;
    public bool sharc;
};

public enum class TonemapOperatorSDR
{
    Linear,
    Aces,
    Reinhard,
    AgX,
    GT7,
};

public enum class TonemapOperatorHDR
{
    Linear,
    Custom,
    Psycho,
};

public struct PostProcessSettings
{
    public TonemapOperatorSDR tonemapperSDR;
    public TonemapOperatorHDR tonemapperHDR;
    public bool hdr;
    public bool autoExposure;
    public float targetExposure;
    public float exposure;
    public float bloomStrength;
}

public struct MipData
{
    public uint2 srcSize;
    public uint2 dstSize;
    public uint mipLevel;
    public uint totalMips;
}

public struct Vertex
{
    public float3 position;
    public float3 normal;
    public float2 uv;
    public float4 tangent;
};

public struct Material
{
    public float4 albedoFactor;
    public int albedoIndex;
    public float3 emissiveFactor;
    public int emissiveIndex;
    public float metallicFactor;
    public float roughnessFactor;
    public int metallicRoughnessIndex;
    public int normalIndex;
    public uint samplerIndex;
    public float ior;
    public uint flags;
    public float3x2 uvTransform;
};

public static const uint MAT_FLAG_TRANSPARENT = (1 << 0);
public static const uint MAT_FLAG_UV_TRANSFORM = (1 << 1);
public static const uint MAT_FLAG_TRANSMISSION = 4;