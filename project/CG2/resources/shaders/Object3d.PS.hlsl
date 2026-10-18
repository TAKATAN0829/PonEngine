#include "Object3d.hlsli"

//=================================================================================================//
struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float32_t shininess;
};
ConstantBuffer<Material> gMaterial : register(b0);
//=================================================================================================//
//=================================================================================================//
struct DirectionalLight
{
    float32_t4 color; // ライトの色
    float32_t3 direction; // ライトの向き
    float intensity; // 輝度
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
//=================================================================================================//
//=================================================================================================//
struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);
//=================================================================================================//
//=================================================================================================//
struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius;
    float decay;
};
ConstantBuffer<PointLight> gPointLight : register(b3);
//=================================================================================================//
//=================================================================================================//
struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t3 direction;
    float32_t distance;
    float32_t decay;
    float32_t cosAngle;
    float32_t cosFalloffStart;
};
ConstantBuffer<SpotLight> gSpotLight : register(b4);
//=================================================================================================//
//=================================================================================================//
struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

Texture2D<float32_t4> gTexture : register(t0);

SamplerState gSampler : register(s0);
//=================================================================================================//
//=================================================================================================//
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // textureのα値が0.5以下のときにPixelを棄却
    if (textureColor.a <= 0.5f)
    {
        discard;
    }
    
    if (gMaterial.enableLighting != 0)
    { // Lightingする場合
        float32_t3 normal = normalize(input.normal);

        float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        //=========================================================================================//
        // 平行光源

        float NdotL = dot(normal, -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        float32_t3 halfVector = normalize(-gDirectionalLight.direction + toEye);
        float specularPow = pow(saturate(dot(normal, halfVector)), gMaterial.shininess);

        float32_t3 diffuseDirectionalLight =
        gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        float32_t3 specularDirectionalLight =
        gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.f, 1.f, 1.f);

        //=========================================================================================//
        // 点光源

        float32_t3 pointLightDirection = normalize(input.worldPosition - gPointLight.position);
        float distance = length(gPointLight.position - input.worldPosition);
        float factor = pow(saturate(-distance / gPointLight.radius + 1.0f), gPointLight.decay);

        float NdotLPoint = dot(normal, -pointLightDirection);
        float cosPoint = pow(NdotLPoint * 0.5f + 0.5f, 2.0f);
        float32_t3 halfVectorPoint = normalize(-pointLightDirection + toEye);
        float specularPowPoint = pow(saturate(dot(normal, halfVectorPoint)), gMaterial.shininess);

        float32_t3 diffusePointLight =
        gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * cosPoint * gPointLight.intensity * factor;
        float32_t3 specularPointLight =
        gPointLight.color.rgb * gPointLight.intensity * factor * specularPowPoint * float32_t3(1.f, 1.f, 1.f);

        //=========================================================================================//
        // スポットライト

        float32_t3 spotLightDirectionOnSurface = normalize(input.worldPosition - gSpotLight.position);
        float distanceSpot = length(gSpotLight.position - input.worldPosition);
        float factorSpot = pow(saturate(-distanceSpot / gSpotLight.distance + 1.0f), gSpotLight.decay);
        float cosAngle = dot(spotLightDirectionOnSurface, gSpotLight.direction);
        float32_t falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));

        float NdotLSpot = dot(normal, -spotLightDirectionOnSurface);
        float cosSpot = pow(NdotLSpot * 0.5f + 0.5f, 2.0f);
        float32_t3 halfVectorSpot = normalize(-spotLightDirectionOnSurface + toEye);
        float specularPowSpot = pow(saturate(dot(normal, halfVectorSpot)), gMaterial.shininess);

        float32_t3 diffuseSpotLight =
        gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * cosSpot * gSpotLight.intensity * factorSpot * falloffFactor;
        float32_t3 specularSpotLight =
        gSpotLight.color.rgb * gSpotLight.intensity * factorSpot * falloffFactor * specularPowSpot * float32_t3(1.f, 1.f, 1.f);

        //=========================================================================================//
        // 合算

        output.color.rgb = diffuseDirectionalLight + specularDirectionalLight + diffusePointLight + specularPointLight + diffuseSpotLight + specularSpotLight;

        output.color.a = gMaterial.color.a * textureColor.a;

    }
    else
    { // Lightingしない場合
        output.color = gMaterial.color * textureColor;
    }

    // output.colorのα値が0.0のときにPixelを棄却
    if (output.color.a == 0.0)
    {
        discard;
    }

    return output;
}
//=================================================================================================//
