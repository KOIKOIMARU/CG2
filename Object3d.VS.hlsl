#include "Object3d.hlsli"

cbuffer TransformCB : register(b0)
{
    float4x4 gWVP;
}

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gWVP);
    output.texcoord = input.texcoord;
    return output;
}