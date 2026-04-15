#include "Object3d.hlsli"

cbuffer TransformCB : register(b1)
{
    float4x4 gWVP;
    float4x4 gWorld;
    float4x4 gWorldInverseTranspose;
};

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gWVP);
    output.texcoord = input.texcoord;
    output.worldPosition = mul(input.position, gWorld).xyz;

    output.normal = normalize(
        mul(input.normal, (float3x3) gWorldInverseTranspose));

    return output;
}
