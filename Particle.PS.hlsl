#include "Particle.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct Material
{
    float32_t4 color;
    int32_t lightingType;     // 0: Lightingなし, 1: Lambert, 2: Half Lambert
    float32_t3 padding;       // C++側の構造体とサイズをそろえるためのパディング
    float32_t4x4 uvTransform; // UV座標変換行列
};
ConstantBuffer<Material> gMaterial : register(b0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};


PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV変換とテクスチャサンプリング
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    output.color = gMaterial.color * textureColor;

    // 最終的なαが0のときは描画しない
    if (output.color.a == 0.0f)
    {
        discard;
    }

    return output;
}