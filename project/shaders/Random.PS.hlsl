#include "Fullscreen.hlsli"

// 時刻で変化する疑似乱数を画面色へ乗算し、ノイズ表現を作る。
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct RandomParameter {
    float32_t time;     // ノイズ模様を変化させる累積時刻
    float32_t3 padding; // 定数バッファの16バイト境界用余白
};

ConstantBuffer<RandomParameter> gRandomParameter : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

float32_t rand2dTo1d(float32_t2 value) {
    float32_t2 dotDir = float32_t2(12.9898f, 78.233f);
    return frac(sin(dot(value, dotDir)) * 43758.5453f);
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    float32_t random = rand2dTo1d(input.texcoord * gRandomParameter.time);
    output.color = gTexture.Sample(gSampler, input.texcoord);
    output.color.rgb *= random;
    output.color.a = 1.0f;
    return output;
}
