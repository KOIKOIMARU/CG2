// UV変換後のテクスチャ色へ、スプライト固有のRGBA色を乗算する。

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor; // テクスチャへ乗算するRGBA色
    int gEnableLighting;   // C++側Materialとのレイアウト互換用
    float3 gPadding;       // 定数バッファ境界用の余白
    float4x4 gUVTransform; // UVの拡縮・移動・反転
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSOutput
{
    float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input)
{
    PSOutput output;

    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gUVTransform).xy;
    float4 tex = gTexture.Sample(gSampler, uv);

    output.color = gMaterialColor * tex;
    return output;
}
