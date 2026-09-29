#include "object3d.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // 位置変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    // UV
    output.texcoord = input.texcoord;
    // 法線変換(逆転置行列を使う)
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    // ワールド座標変換
    output.worldPosition = mul(input.position, gTransformationMatrix.World).xyz;
    
    return output;
}