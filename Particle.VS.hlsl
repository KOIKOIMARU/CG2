#include "Particle.hlsli"

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

cbuffer TransformCB : register(b1)
{
    float4x4 gViewProjection;
};

StructuredBuffer<InstanceData> gInstances : register(t1);

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 color : COLOR0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    InstanceData inst = gInstances[instanceId];

    float4 worldPos = mul(input.position, inst.world);
    output.position = mul(worldPos, gViewProjection);

    output.texcoord = input.texcoord;
    output.normal = normalize(input.normal);
    output.color = inst.color;

    return output;
}
