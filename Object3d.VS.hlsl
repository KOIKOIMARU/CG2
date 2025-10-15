cbuffer TransformCB : register(b1)
{
    float4x4 gWVP;
    float4x4 gWorld;
    float4x4 gWorldInverseTranspose;
};

struct VSIn
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};
struct VSOut
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VSOut main(VSIn i)
{
    VSOut o;
    o.position = mul(i.position, gWVP);
    // 3x3 に切り出して法線変換
    float3x3 N = (float3x3) gWorldInverseTranspose;
    o.normal = normalize(mul(i.normal, N));
    o.texcoord = i.texcoord;
    return o;
}
