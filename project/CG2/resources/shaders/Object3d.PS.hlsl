#include "Object3d.hlsli"
#include "SceneLights.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float32_t shininess;
};
ConstantBuffer<Material> gMaterial : register(b0);
struct Camera {
    float3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);
struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    float4 transformedUV = mul(float4(input.texcoord,0,1),gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler,transformedUV.xy);
    if (textureColor.a <= 0.5f) {
        discard;
    }
    output.color = gMaterial.color*textureColor;
    if (gMaterial.enableLighting != 0) {
        float3 normal = input.normal/max(length(input.normal),0.00001f);
        float3 toEye = gCamera.worldPosition-input.worldPosition;
        toEye /= max(length(toEye),0.00001f);
        output.color.rgb = CalculateSceneLighting(input.worldPosition,normal,toEye,output.color.rgb,gMaterial.shininess);
    }
    if (output.color.a == 0) {
        discard;
    }
    return output;
}
