#include "object3d.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct Material
{
    float32_t4 color;
    int32_t lightingType;     // 0: Lightingなし, 1: Lambert, 2: Half Lambert
    float32_t3 padding;       // C++側の構造体とサイズをそろえるためのパディング
    float32_t4x4 uvTransform; // UV座標変換行列
    float32_t shininess;      // 光沢度
};
ConstantBuffer<Material> gMaterial : register(b0);


struct DirectionalLight
{
    float32_t4 color;     // ライトの色
    float32_t3 direction; // ライトの向き（単位ベクトル
    float intensity;      // 輝度
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

// カメラ位置をGPUへ送る構造体
struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

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

    // アルファ値が0.5以下の場合は描画しない
    if (textureColor.a <= 0.5f)
    {
        discard;
    }
    
    // カメラへの方向 (数式の v)
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    
    // --- Phong(比較用)
    //loat32_t3 reflectLight = reflect(gDirectionalLight.direction, normalize(input.normal));
    //float RdotE = dot(reflectLight, toEye);
    //float specularPow = pow(saturate(RdotE), gMaterial.shininess);
    
     // --- Blinn-Phong ---
    float32_t3 halfVector = normalize(-gDirectionalLight.direction + toEye);
    float NDotH = dot(normalize(input.normal), halfVector);
    float specularPow = pow(saturate(NDotH), gMaterial.shininess);
    
    // 0: Lightingなし
    if (gMaterial.lightingType == 0)
    {
        output.color = gMaterial.color * textureColor;
    }
    // 1: Lambert
    else if (gMaterial.lightingType == 1)
    {
        // N dot L
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = saturate(NdotL); 
        
        // 拡散反射
        float32_t3 diffuse =
            gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        // 鏡面反射
        float32_t3 specular =
            gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
        
        // 拡散反射 + 鏡面反射
        output.color.rgb = diffuse + specular;
        // a はライティングの影響を受けないように個別に計算
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    // 2: Half Lambert
    else if (gMaterial.lightingType == 2)
    {
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        // NdotL (-1.0 ～ 1.0) を (0.0 ～ 1.0) に変換して2乗
        float halfLambert = pow((NdotL * 0.5f) + 0.5f, 2.0f);
        
         // 拡散反射
        float32_t3 diffuse =
            gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * halfLambert * gDirectionalLight.intensity;
        // 鏡面反射 
        float32_t3 specular =
            gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
        
       // 拡散反射 + 鏡面反射
        output.color.rgb = diffuse + specular;
        // a はライティングの影響を受けないように個別に計算
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    return output;
}