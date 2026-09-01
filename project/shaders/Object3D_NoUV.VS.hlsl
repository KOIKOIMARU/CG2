// UVを持たない単色モデルを通常3Dパイプラインへ渡す頂点シェーダー。
struct VSInput
{
    float4 position : POSITION;
    float3 normal : NORMAL;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0; // Pixel Shaderとの共通入出力を保つダミーUV
    float3 normal : NORMAL0;
};

cbuffer TransformCB : register(b1)
{
    float4x4 WVP;
    float4x4 World;
};

VertexShaderOutput main(VSInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, WVP);
    output.normal = mul((float3x3) World, input.normal);
    output.texcoord = float2(0.0f, 0.0f);
    return output;
}
