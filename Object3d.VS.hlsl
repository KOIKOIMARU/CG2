#include "Object3d.hlsli"

cbuffer TransformCB : register(b1)
{
    float4x4 gWVP;
    float4x4 gWorld;
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

    float4 worldPos = mul(input.position, gWorld);
    output.worldPos = worldPos.xyz;

    float3x3 normalMatrix = (float3x3) gWorld;
    output.normal = input.normal;
    output.worldNormal = normalize(mul(input.normal, normalMatrix));

    return output;
}
