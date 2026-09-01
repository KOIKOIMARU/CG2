// ピクセル単位で指定されたスプライト頂点を画面のクリップ座標へ変換する。

cbuffer TransformCB : register(b1)
{
    float4x4 gWVP;   // スプライトのローカル座標からクリップ座標への変換
    float4x4 gWorld; // C++側TransformDataとのレイアウト互換用
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
