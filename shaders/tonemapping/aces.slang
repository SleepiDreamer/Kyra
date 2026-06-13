module aces;

// based on https://64.github.io/tonemapping/
static const float3x3 acesInputMatrix = float3x3(
    0.59719f, 0.35458f, 0.04823f,
    0.07600f, 0.90834f, 0.01566f,
    0.02840f, 0.13383f, 0.83777f
);

static const float3x3 acesOutputMatrix = float3x3(
    1.60475f, -0.53108f, -0.07367f,
   -0.10208f, 1.10813f, -0.00605f,
   -0.00327f, -0.07276f, 1.07602f
);

float3 rttAndOdtFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

public float3 TonemapACES(float3 color)
{
    color = mul(acesInputMatrix, color);
    color = rttAndOdtFit(color);
    return mul(acesOutputMatrix, color);
}