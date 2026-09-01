#include "Object3d.hlsli"

// CPUまたはVertex Shaderスキニング後の頂点をワールド・クリップ空間へ変換する。
cbuffer TransformCB : register(b1)
{
    float4x4 gWVP;                   // ローカル座標からクリップ座標への変換
    float4x4 gWorld;                 // ローカル座標からワールド座標への変換
    float4x4 gWorldInverseTranspose; // 法線をワールド空間へ変換する行列
};

cbuffer SkinningCB : register(b6)
{
    int gEnableSkinning;             // Vertex Shaderでスキニングする場合は1
    float3 gSkinningPadding;         // 定数バッファ境界用の余白
    WellForGPU gMatrixPalette[128];  // ジョイント番号順の現在姿勢
};

struct VertexShaderInput
{
    float4 position : POSITION0;       // モデル空間の頂点位置
    float2 texcoord : TEXCOORD0;       // モデルのUV座標
    float3 normal : NORMAL0;           // モデル空間の頂点法線
    float4 weight : BLENDWEIGHT0;      // 最大4ジョイントの影響率
    uint4 jointIndices : BLENDINDICES0;// weightに対応するジョイント番号
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float4 skinnedPosition = input.position;
    float3 skinnedNormal = input.normal;

    if (gEnableSkinning != 0)
    {
        skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
        skinnedNormal = float3(0.0f, 0.0f, 0.0f);

        if (input.weight.x > 0.0f)
        {
            skinnedPosition += input.weight.x * mul(input.position, gMatrixPalette[input.jointIndices.x].skeletonSpaceMatrix);
            skinnedNormal += input.weight.x * mul(input.normal, (float3x3)gMatrixPalette[input.jointIndices.x].skeletonSpaceInverseTransposeMatrix);
        }
        if (input.weight.y > 0.0f)
        {
            skinnedPosition += input.weight.y * mul(input.position, gMatrixPalette[input.jointIndices.y].skeletonSpaceMatrix);
            skinnedNormal += input.weight.y * mul(input.normal, (float3x3)gMatrixPalette[input.jointIndices.y].skeletonSpaceInverseTransposeMatrix);
        }
        if (input.weight.z > 0.0f)
        {
            skinnedPosition += input.weight.z * mul(input.position, gMatrixPalette[input.jointIndices.z].skeletonSpaceMatrix);
            skinnedNormal += input.weight.z * mul(input.normal, (float3x3)gMatrixPalette[input.jointIndices.z].skeletonSpaceInverseTransposeMatrix);
        }
        if (input.weight.w > 0.0f)
        {
            skinnedPosition += input.weight.w * mul(input.position, gMatrixPalette[input.jointIndices.w].skeletonSpaceMatrix);
            skinnedNormal += input.weight.w * mul(input.normal, (float3x3)gMatrixPalette[input.jointIndices.w].skeletonSpaceInverseTransposeMatrix);
        }
    }

    output.position = mul(skinnedPosition, gWVP);
    output.texcoord = input.texcoord;
    output.worldPosition = mul(skinnedPosition, gWorld).xyz;

    output.normal = normalize(
        mul(skinnedNormal, (float3x3) gWorldInverseTranspose));

    return output;
}
