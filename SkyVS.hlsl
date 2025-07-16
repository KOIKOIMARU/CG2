cbuffer CameraCB : register(b1)
{
    float4x4 gView;
    float4x4 gProjection;
    float3 gCameraPos;
    float padding; // 16バイト揃え
};

struct VSInput
{
    float4 position : POSITION;
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

    // 頂点位置をカメラ位置にオフセット（回転は無視）
    float3 pos = input.position.xyz + gCameraPos;

    // ビュー行列で変換
    float4 viewPos = mul(float4(pos, 1.0f), gView);

    // 射影変換
    output.position = mul(viewPos, gProjection);

    output.texcoord = input.texcoord;

    return output;
}
