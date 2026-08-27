module settings;

// Ray Tracing
public export static const float MAX_RAY_DEPTH = 65536.0;
public static const float NEAR_PLANE = 0.01;
public static const float FAR_PLANE = 100.0;

public export static const bool SER = true;

public export static const bool USE_RUSSIAN_ROULETTE = false;
public export static const uint RUSSIAN_ROULETTE_START_BOUNCE = 0;

public export static const bool DEPTH_OF_FIELD = true;

public export static const float BEER_ABSORPTION = 0.5;

// SHaRC
public export static const uint SHARC_CAPACITY = 1 << 22;        // has to match SHARC_CAPACITY in Renderer.cpp
public export static const float SHARC_SCENE_SCALE = 128.0;       // voxel size is roughly distance-to-camera / sceneScale, so this is scene dependent
public export static const float SHARC_RADIANCE_SCALE = 1e3;     // quantization factor for the atomic radiance accumulation
public export static const uint SHARC_ACCUMULATION_FRAMES = 64;  // temporal accumulation window of a cache entry
public export static const uint SHARC_STALE_FRAMES = 128;         // frames without new samples before an entry is evicted
public export static const uint SHARC_UPDATE_STRIDE = 4;         // one update ray per NxN pixel tile
public export static const uint SHARC_UPDATE_RNG_OFFSET = 0x9E3779B9; // keeps update rays off the render pass' sample sequence
public export static const uint SHARC_QUERY_MIN_DEPTH = 1;       // first bounce that is allowed to read from the cache
public export static const float SHARC_MIN_ROUGHNESS = 0.0005;   // squared roughness, so this only excludes near-perfect mirrors

// Auto Exposure
public export static const int AE_SAMPLES = 32;
public export static const float AE_MIN_EXPOSURE = 0.5;
public export static const float AE_MAX_EXPOSURE = 2000.0;
public export static const float AE_ADAPTATION_SPEED_UP = 0.5;
public export static const float AE_ADAPTATION_SPEED_DOWN = 5.0;

// Auto Focus
public export static const int AF_SPEED = 8;

// Bloom
public static const float bloomFilterRadius = 0.001;