#include "Particle.hlsli"

struct ParticleForGPU
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4 color;
};

StructuredBuffer<ParticleForGPU> gParticles : register(t0); // パーティクルデータのStructuredBuffer

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    // WVP行列を使用して頂点の位置を変換
    output.position = mul(input.position, gParticles[instanceId].WVP);
    // UV座標をそのまま出力
    output.texcoord = input.texcoord;
    // 法線ベクトルをワールド座標系に変換
    output.normal = normalize(mul(input.normal, (float32_t3x3) gParticles[instanceId].World));
    // パーティクルの色を出力
    output.color = gParticles[instanceId].color;
    return output;
}