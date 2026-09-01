#include "Object3d.hlsli"

// 頂点スキニングをGPUで並列処理し、通常描画用の頂点バッファへ書き出す。
struct Vertex
{
    float4 position; // 変形前のモデル空間位置
    float2 texcoord; // 変形せず出力へ引き継ぐUV座標
    float3 normal;   // 変形前のモデル空間法線
};

struct VertexInfluence
{
    float4 weight; // 最大4ジョイントの影響率
    int4 index;    // weightに対応するジョイント番号
};

struct OutputVertex
{
    float4 position;    // 現在姿勢へ変形済みの頂点位置
    float2 texcoord;    // 元頂点のUV座標
    float3 normal;      // 現在姿勢へ変形済みの法線
    float4 weight;      // 後段とのレイアウト互換用の影響率
    uint4 jointIndices; // 後段とのレイアウト互換用のジョイント番号
};

cbuffer SkinningInformation : register(b0)
{
    uint gNumVertices;                  // 実際に処理する頂点数
    float3 gSkinningInformationPadding; // 定数バッファ境界用の余白
};

StructuredBuffer<WellForGPU> gMatrixPalette : register(t0);
StructuredBuffer<Vertex> gInputVertices : register(t1);
StructuredBuffer<VertexInfluence> gInfluences : register(t2);
RWStructuredBuffer<OutputVertex> gOutputVertices : register(u0);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vertexIndex = DTid.x;
    if (vertexIndex >= gNumVertices)
    {
        return;
    }

    Vertex input = gInputVertices[vertexIndex];
    VertexInfluence influence = gInfluences[vertexIndex];

    OutputVertex skinned;
    skinned.texcoord = input.texcoord;
    skinned.weight = influence.weight;
    skinned.jointIndices = uint4(
        influence.index.x,
        influence.index.y,
        influence.index.z,
        influence.index.w);

    float4 skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    if (influence.weight.x > 0.0f)
    {
        skinnedPosition += influence.weight.x *
            mul(input.position, gMatrixPalette[influence.index.x].skeletonSpaceMatrix);
        skinnedNormal += influence.weight.x *
            mul(input.normal, (float3x3)gMatrixPalette[influence.index.x].skeletonSpaceInverseTransposeMatrix);
    }
    if (influence.weight.y > 0.0f)
    {
        skinnedPosition += influence.weight.y *
            mul(input.position, gMatrixPalette[influence.index.y].skeletonSpaceMatrix);
        skinnedNormal += influence.weight.y *
            mul(input.normal, (float3x3)gMatrixPalette[influence.index.y].skeletonSpaceInverseTransposeMatrix);
    }
    if (influence.weight.z > 0.0f)
    {
        skinnedPosition += influence.weight.z *
            mul(input.position, gMatrixPalette[influence.index.z].skeletonSpaceMatrix);
        skinnedNormal += influence.weight.z *
            mul(input.normal, (float3x3)gMatrixPalette[influence.index.z].skeletonSpaceInverseTransposeMatrix);
    }
    if (influence.weight.w > 0.0f)
    {
        skinnedPosition += influence.weight.w *
            mul(input.position, gMatrixPalette[influence.index.w].skeletonSpaceMatrix);
        skinnedNormal += influence.weight.w *
            mul(input.normal, (float3x3)gMatrixPalette[influence.index.w].skeletonSpaceInverseTransposeMatrix);
    }

    skinned.position = skinnedPosition;
    skinned.normal = normalize(skinnedNormal);
    gOutputVertices[vertexIndex] = skinned;
}
