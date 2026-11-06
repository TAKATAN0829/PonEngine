#include "Particle.hlsli"

struct Material
{
    float32_t4 color;

    int32_t enableLighting;

    float32_t4x4 uvTransform;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b0);

// C++のBlendModeと同じ値。Particle専用Root ConstantsでDrawごとに指定する。
cbuffer ParticleSurface : register(b1)
{
    uint32_t useBrightnessAlpha;
    uint32_t blendMode;
    uint32_t softEnabled;

    float32_t softDistance;

    row_major float32_t4x4 inverseViewProjection;

    float32_t2 depthSize;
    float32_t2 surfacePadding;
};

static const uint32_t kBlendMultiply = 4;
static const uint32_t kBlendScreen = 5;
static const uint32_t kBlendLighten = 6;
static const uint32_t kBlendDarken = 7;
static const uint32_t kBlendInvert = 8;

Texture2D<float32_t4> gTexture : register(t0);

Texture2D<float32_t> gSceneDepth : register(t1);

SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

	// Alphaがない発光画像は明るさを透明度へ移し、黒い四角い背景を残さない。
	// RGBを同じ明るさで割るため、Addでの発光量を二重に減衰させない。
    if (useBrightnessAlpha != 0)
    {
        float32_t brightness = max(textureColor.r, max(textureColor.g, textureColor.b));

        textureColor.a *= brightness;

        textureColor.rgb /= max(brightness, 0.00001f);
    }

    output.color = gMaterial.color * textureColor * input.color;

	output.color.a = saturate(output.color.a);

    // SV_POSITIONは現在の描画先のピクセル座標。深度は同じカメラの逆行列で復元する。
    if (softEnabled != 0)
    {
        float32_t sceneDepth = gSceneDepth.Load(int32_t3(int32_t2(input.position.xy), 0));

        // 深度が初期値の場所には背景の物体がないため、交差によるフェードはかけない。
        if (sceneDepth < 1.0f)
        {
            float32_t2 normalizedPosition = input.position.xy / depthSize * float32_t2(2, -2) + float32_t2(-1, 1);

            float32_t4 sceneWorld = mul(float32_t4(normalizedPosition, sceneDepth, 1), inverseViewProjection);
            float32_t4 particleWorld = mul(float32_t4(normalizedPosition, input.position.z, 1), inverseViewProjection);

            float32_t separation = length(sceneWorld.xyz / sceneWorld.w - particleWorld.xyz / particleWorld.w);

            // 背景との距離が指定値より近いほど透明にし、粒子の板と物体の境目をぼかす。
            output.color.a *= saturate(separation / softDistance);
        }
    }

    // output.colorのα値が0のときにPixelを棄却
    if (output.color.a <= 0.00001f)
    {
        discard;
    }

    // SrcAlphaを使わない合成にも、画像のマスク・色Alpha・寿命フェードを反映する。
    if (blendMode == kBlendScreen || blendMode == kBlendLighten || blendMode == kBlendInvert)
    {
        output.color.rgb *= output.color.a;
    }
    else if (blendMode == kBlendMultiply || blendMode == kBlendDarken)
    {
        output.color.rgb = lerp(float32_t3(1, 1, 1), output.color.rgb, output.color.a);
    }

    return output;
}
