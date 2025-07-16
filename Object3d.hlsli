#ifndef OBJECT3D_HLSLI
#define OBJECT3D_HLSLI

struct Material
{
    float4 color;
    int enableLighting;
    float crackAmount;
    float2 padding;
    float4x4 uvTransform;
};


struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
    float3 worldPos : TEXCOORD3;
};

#endif
