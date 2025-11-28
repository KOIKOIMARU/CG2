struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 color : COLOR0;
};

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor;
    int gLightingMode;
    float3 _padding;
    float4x4 gUVTransform;
};

cbuffer TransformCB : register(b1)
{
    float4x4 gViewProjection;
};

struct DirectionalLight
{
    float4 color;
    float4 direction;
    float intensity;
    float3 padding;
};

cbuffer DirectionalLightCB : register(b3)
{
    DirectionalLight gDirectionalLight;
};

struct InstanceData
{
    float4x4 world;
    float4 color;
};

StructuredBuffer<InstanceData> gInstances : register(t1);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
