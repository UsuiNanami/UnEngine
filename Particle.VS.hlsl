#include "Particle.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
};

StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    // 変換行列を使用して頂点の位置を変換
    VertexShaderOutput output;
    // WVP行列を使用して頂点の位置を変換
    output.position = mul(input.position, gTransformationMatrices[instanceId].WVP);
    // UV座標と法線ベクトルをそのまま出力
    output.texcoord = input.texcoord;
    // 法線ベクトルをワールド座標系に変換
    output.normal = normalize(mul(input.normal, (float32_t3x3) gTransformationMatrices[instanceId].World));
    return output;
}