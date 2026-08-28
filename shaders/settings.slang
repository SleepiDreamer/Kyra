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
public export static const uint SHARC_CAPACITY = 1 << 22;
public export static const float SHARC_SCENE_SCALE = 64.0;
public export static const float SHARC_RADIANCE_SCALE = 1e3;
public export static const uint SHARC_ACCUMULATION_FRAMES = 32;
public export static const uint SHARC_STALE_FRAMES = 64;
public export static const uint SHARC_UPDATE_STRIDE = 5;
public export static const uint SHARC_UPDATE_RNG_OFFSET = 0x9E3779B9;
public export static const uint SHARC_QUERY_MIN_DEPTH = 1;
public export static const float SHARC_MIN_ROUGHNESS = 0.0005;

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