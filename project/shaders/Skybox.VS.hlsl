#include "Skybox.hlsli"

// カメラ移動を除いた行列で背景キューブを描き、常に最奥の深度へ配置する。
cbuffer ViewProjectionCB : register(b0)
{
    float4x4 gViewProjection; // 平行移動を除いたビュー・投影合成行列
};

struct VertexShaderInput
{
    float4 position : POSITION0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gViewProjection);

    // z/wを1にして、他のすべての3D物体より奥へ配置する。
    output.position.z = output.position.w;

    // キューブ中心から頂点への方向をキューブマップの参照方向にする。
    output.texcoord = input.position.xyz;
    return output;
}
