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

// 平行光源構造体
struct DirectionalLight
{
    float32_t4 color;     // ライトの色
    float32_t3 direction; // ライトの向き（単位ベクトル
    float intensity;      // 輝度
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

// 点光源構造体
struct PointLight
{
    float32_t4 color;    // 点光源の色
    float32_t3 position; // 点光源の位置
    float intensity;     // 点光源の強度
    float radius; // 点光源の有効範囲
    float decay; // 減衰係数
};
ConstantBuffer<PointLight> gPointLight : register(b3);

// スポットライト構造体
struct SpotLight
{
    float32_t4 color;     // スポットライトの色
    float32_t3 position;  // スポットライトの位置
    float intensity;      // スポットライトの強度
    float32_t3 direction; // スポットライトの方向
    float distance;       // スポットライトの有効距離
    float decay;          // 減衰係数
    float cosAngle;       // スポットライトの角度（ラジアン）
    float cosFalloffStart; // Falloff開始角度（ラジアン） falloffFactor
}; 
ConstantBuffer<SpotLight> gSpotLight : register(b4);


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
    
    // --- PointLight の入射光 ---
    float32_t3 pointLightDirection = normalize(input.worldPosition - gPointLight.position);

    float32_t distance = length(gPointLight.position - input.worldPosition);
    float32_t factor = pow(saturate(-distance / gPointLight.radius + 1.0f), gPointLight.decay);

    float32_t3 pointLightColor = gPointLight.color.rgb * gPointLight.intensity * factor;
    
    
     // --- SpotLight の入射光 --- 
    float32_t3 spotLightDirectionOnSurface = normalize(input.worldPosition - gSpotLight.position);

    // 距離による減衰(PointLightと同じ考え方)
    float32_t spotDistance = length(gSpotLight.position - input.worldPosition);
    float32_t attenuationFactor = pow(saturate(-spotDistance / gSpotLight.distance + 1.0f), gSpotLight.decay);

   // 角度による減衰(Falloff)
    float32_t cosAngle = dot(spotLightDirectionOnSurface, gSpotLight.direction);
    float32_t falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));

    float32_t3 spotLightColor = gSpotLight.color.rgb * gSpotLight.intensity * attenuationFactor * falloffFactor;
    
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
        
        // 拡散反射(DirectionalLight)
        float32_t3 diffuseDirectionalLight =
         gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        // 鏡面反射(DirectionalLight)
        float32_t3 specularDirectionalLight =
         gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
        
        // PointLight用の N dot L
        float NdotLPoint = dot(normalize(input.normal), -pointLightDirection);
        float cosPoint = saturate(NdotLPoint);

        // PointLight用のハーフベクトルと鏡面反射の強さ
        float32_t3 halfVectorPoint = normalize(-pointLightDirection + toEye);
        float NDotHPoint = dot(normalize(input.normal), halfVectorPoint);
        float specularPowPoint = pow(saturate(NDotHPoint), gMaterial.shininess);

        float32_t3 diffusePointLight =
         gMaterial.color.rgb * textureColor.rgb * pointLightColor * cosPoint;
        float32_t3 specularPointLight =
         pointLightColor * specularPowPoint * float32_t3(1.0f, 1.0f, 1.0f);

        // ===== SpotLight =====
        float NdotLSpot = dot(normalize(input.normal), -spotLightDirectionOnSurface);
        float cosSpot = saturate(NdotLSpot);

        float32_t3 halfVectorSpot = normalize(-spotLightDirectionOnSurface + toEye);
        float NDotHSpot = dot(normalize(input.normal), halfVectorSpot);
        float specularPowSpot = pow(saturate(NDotHSpot), gMaterial.shininess);

        float32_t3 diffuseSpotLight =
         gMaterial.color.rgb * textureColor.rgb * spotLightColor * cosSpot;
        float32_t3 specularSpotLight =
         spotLightColor * specularPowSpot * float32_t3(1.0f, 1.0f, 1.0f);
        
                // ===== 全部足す ===== 
        output.color.rgb = diffuseDirectionalLight + specularDirectionalLight
                      + diffusePointLight + specularPointLight
                      + diffuseSpotLight + specularSpotLight;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    // 2: Half Lambert
    else if (gMaterial.lightingType == 2)
    {
     // ===== DirectionalLight =====
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float halfLambert = pow((NdotL * 0.5f) + 0.5f, 2.0f);

        // 拡散反射(DirectionalLight)
        float32_t3 diffuseDirectionalLight =
         gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * halfLambert * gDirectionalLight.intensity;
        // 鏡面反射(DirectionalLight)
        float32_t3 specularDirectionalLight =
         gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);

        // ===== PointLight ===== 
        float NdotLPoint = dot(normalize(input.normal), -pointLightDirection);
        float halfLambertPoint = pow((NdotLPoint * 0.5f) + 0.5f, 2.0f);

        float32_t3 halfVectorPoint = normalize(-pointLightDirection + toEye);
        float NDotHPoint = dot(normalize(input.normal), halfVectorPoint);
        float specularPowPoint = pow(saturate(NDotHPoint), gMaterial.shininess);

        float32_t3 diffusePointLight =
         gMaterial.color.rgb * textureColor.rgb * pointLightColor * halfLambertPoint;
        float32_t3 specularPointLight =
         pointLightColor * specularPowPoint * float32_t3(1.0f, 1.0f, 1.0f);

        // ===== SpotLight =====
        float NdotLSpot = dot(normalize(input.normal), -spotLightDirectionOnSurface);
        float halfLambertSpot = pow((NdotLSpot * 0.5f) + 0.5f, 2.0f);

        float32_t3 halfVectorSpot = normalize(-spotLightDirectionOnSurface + toEye);
        float NDotHSpot = dot(normalize(input.normal), halfVectorSpot);
        float specularPowSpot = pow(saturate(NDotHSpot), gMaterial.shininess);

        float32_t3 diffuseSpotLight =
         gMaterial.color.rgb * textureColor.rgb * spotLightColor * halfLambertSpot; // ← halfLambertSpotに
        float32_t3 specularSpotLight =
         spotLightColor * specularPowSpot * float32_t3(1.0f, 1.0f, 1.0f);
        
        // ===== 全部足す =====
        output.color.rgb = diffuseDirectionalLight + specularDirectionalLight
                      + diffusePointLight + specularPointLight
                      + diffuseSpotLight + specularSpotLight;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    return output;
}