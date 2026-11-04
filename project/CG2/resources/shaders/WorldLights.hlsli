// LightStructs.hと同じ配列長・16バイト境界を使う。ライトの追加はWorldLightingで行う。
struct DirectionalLight {
    float4 color;

    float3 direction;

    float intensity;
};

struct PointLight {
    float4 color;

    float3 position;

    float intensity;
    float radius;
    float decay;
};

struct SpotLight {
    float4 color;

    float3 position;

    float intensity;

    float3 direction;

    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
};

struct DirectionalLightBuffer {
    uint count;

    float3 padding;

    DirectionalLight lights[4];
};

struct PointLightBuffer {
    uint count;

    float3 padding;

    PointLight lights[9];
};

struct SpotLightBuffer {
    uint count;

    float3 padding;

    SpotLight lights[8];
};

ConstantBuffer<DirectionalLightBuffer> gDirectionalLights : register(b1);

ConstantBuffer<PointLightBuffer> gPointLights : register(b3);

ConstantBuffer<SpotLightBuffer> gSpotLights : register(b4);

// 従来のHalf LambertとBlinn-Phongを各ライトで共用する。
float3 CalculateLightContribution(float3 baseColor, float3 normal, float3 toEye, float3 toLight, float3 radiance, float shininess) {
    float diffuseFactor = pow(dot(normal,toLight)*0.5f+0.5f,2.0f);

    float3 halfDirection = toLight+toEye;

    halfDirection /= max(length(halfDirection),0.00001f);

    float specularFactor = pow(saturate(dot(normal,halfDirection)),shininess);

    return (baseColor*diffuseFactor+specularFactor)*radiance;
}

float3 CalculateWorldLighting(float3 position, float3 normal, float3 toEye, float3 baseColor, float shininess) {
    float3 lighting = float3(0,0,0);

    for (uint lightIndex = 0; lightIndex < min(gDirectionalLights.count,4u); ++lightIndex) {
        DirectionalLight light = gDirectionalLights.lights[lightIndex];

        lighting += CalculateLightContribution(baseColor,normal,toEye,-light.direction,light.color.rgb*light.intensity,shininess);
    }

    for (uint lightIndex = 0; lightIndex < min(gPointLights.count,9u); ++lightIndex) {
        PointLight light = gPointLights.lights[lightIndex];

        float3 offset = light.position-position;

        float distanceToLight = length(offset);
        float attenuation = pow(saturate(1-distanceToLight/light.radius),light.decay);

        if (attenuation > 0) {
            float3 toLight = offset/max(distanceToLight,0.00001f);

            lighting += CalculateLightContribution(baseColor,normal,toEye,toLight,light.color.rgb*light.intensity*attenuation,shininess);
        }
    }

    for (uint lightIndex = 0; lightIndex < min(gSpotLights.count,8u); ++lightIndex) {
        SpotLight light = gSpotLights.lights[lightIndex];

        float3 fromLight = position-light.position;

        float distanceToLight = length(fromLight);

        fromLight /= max(distanceToLight,0.00001f);

        float attenuation = pow(saturate(1-distanceToLight/light.distance),light.decay);
        float coneCosine = dot(fromLight,light.direction);
        float coneWidth = light.cosFalloffStart-light.cosAngle;
        float coneFactor = step(light.cosAngle,coneCosine);

        if (coneWidth > 0.00001f) {
            coneFactor = saturate((coneCosine-light.cosAngle)/coneWidth);
        }

        if (attenuation*coneFactor > 0) {
            lighting += CalculateLightContribution(baseColor,normal,toEye,-fromLight,
                light.color.rgb*light.intensity*attenuation*coneFactor,shininess);
        }
    }

    return lighting;
}
