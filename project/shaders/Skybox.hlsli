// Skyboxの頂点・ピクセルシェーダー間で共有する入出力。
struct VertexShaderOutput
{
    float4 position : SV_POSITION; // 射影後のクリップ座標
    float3 texcoord : TEXCOORD0;   // キューブマップを読む方向ベクトル
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0; // 背景として出力するRGBA色
};
