struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct Material
{
    float4 color;
    int enableLighting;
    int padding[3];
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};