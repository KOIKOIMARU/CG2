// Sprite.VS.hlsl

cbuffer TransformCB : register(b1)
{
    float4x4 gWVP;
    float4x4 gWorld; // 使わないけどC++側のTransformDataがWorld/WVPなら合わせておく
};

struct VSInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(gWVP, input.position);
    output.texcoord = input.texcoord;
    return output;
}
