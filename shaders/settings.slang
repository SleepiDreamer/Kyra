module settings;

// Ray Tracing
public export static const float MAX_RAY_DEPTH = 65536.0;
public static const float NEAR_PLANE = 0.01;
public static const float FAR_PLANE = 100.0;

public export static const bool USE_RUSSIAN_ROULETTE = false;
public export static const uint RUSSIAN_ROULETTE_START_BOUNCE = 0;

public export static const bool DEPTH_OF_FIELD = true;

public export static const float BEER_ABSORPTION = 5.0;

// Auto Exposure
public export static const int AE_SAMPLES = 32;
public export static const float MIN_EXPOSURE = 0.1;
public export static const float MAX_EXPOSURE = 200.0;
public export static const float ADAPTATION_SPEED = 2.5;