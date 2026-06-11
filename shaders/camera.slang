module camera;

import structs;
import rng;
import random.sampling;

public struct Camera
{
    public float3 PinholeCamera(float2 uv, CameraData camera)
    {
        float2 ndc = (uv * 2.0f) - 1.0f;
        float2 resolution = DispatchRaysDimensions().xy;
        float aspectRatio = resolution.x / resolution.y;

        float x = ndc.x * aspectRatio * tan(radians(camera.fov) * 0.5);
        float y = -ndc.y * tan(radians(camera.fov) * 0.5);

        float3 rayDir = normalize(float3(x, y, 1.0));
        float3 rotatedRayDir = rayDir.x * camera.right + rayDir.y * camera.up + rayDir.z * camera.forward;

        return normalize(rotatedRayDir);
    }

    public float3 ThinLensCamera(inout RNG rng, float2 uv, CameraData camera, out float3 origin)
    {
        float2 ndc = (uv * 2.0f) - 1.0f;
        float2 resolution = DispatchRaysDimensions().xy;
        float aspectRatio = resolution.x / resolution.y;

        float x = ndc.x * aspectRatio * tan(radians(camera.fov) * 0.5);
        float y = -ndc.y * tan(radians(camera.fov) * 0.5);
        float3 rayDir = normalize(float3(x, y, 1.0));
        rayDir = rayDir.x * camera.right + rayDir.y * camera.up + rayDir.z * camera.forward;

        float2 offset = sampleDisk(rng.nextFloat2()) * camera.aperture * 0.01;
        float3 rayPos = camera.position + offset.x * camera.right + offset.y * camera.up;
        origin = rayPos;

        float3 focusPoint = camera.position + rayDir * camera.focusDistance;
        rayDir = normalize(focusPoint - rayPos);

        return normalize(rayDir);
    }

    public float3 AnamorphicCamera(inout RNG rng, float2 uv, CameraData camera, float swirliness, float stretch, out float3 origin)
    {
        float2 ndc = (uv * 2.0f) - 1.0f;
        float2 resolution = DispatchRaysDimensions().xy;
        float aspectRatio = resolution.x / resolution.y;

        float x = ndc.x * aspectRatio * tan(radians(camera.fov) * 0.5);
        float y = -ndc.y * tan(radians(camera.fov) * 0.5);
        
        float3 rayDir = normalize(float3(x, y, 1.0));
        rayDir = rayDir.x * camera.right + rayDir.y * camera.up + rayDir.z * camera.forward;

        float2 offset = 0;

        for (int samples = 10; samples > 0; --samples)
        {
            offset = sampleDisk(rng.nextFloat2());

            float2 centerDeviation = (uv - 0.5) * 2.0;
            centerDeviation.y /= -aspectRatio;
            float2 sampleUV = uv + offset * 0.01;
            float2 vignetteOrigin = uv - centerDeviation * 0.01;

            if (!(distance(sampleUV, vignetteOrigin) > 0.01 / swirliness))
            {
                break;
            }
        }

        offset *= camera.aperture * 0.01;
        offset.x /= stretch;

        float3 rayPos = camera.position + offset.x * camera.right + offset.y * camera.up;
        origin = rayPos;

        float3 focusPoint = camera.position + rayDir * camera.focusDistance;
        rayDir = normalize(focusPoint - rayPos);

        return normalize(rayDir);
    }
}
