#include "Fullscreen.hlsli"

// 指定中心へ向かう複数点を平均し、速度演出用の放射ブラーを作る。
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct RadialBlurParameter {
    float32_t2 center;   // ブラーが収束するUV座標
    float32_t blurWidth; // サンプル間隔を決める強度
    float32_t padding;   // 定数バッファの16バイト境界用余白
};

ConstantBuffer<RadialBlurParameter> gRadialBlurParameter : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    const int32_t kNumSamples = 10;

    float32_t2 direction = input.texcoord - gRadialBlurParameter.center;
    float32_t4 outputColor = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int32_t sampleIndex = 0; sampleIndex < kNumSamples; ++sampleIndex) {
        float32_t2 texcoord =
            input.texcoord - direction *
            gRadialBlurParameter.blurWidth * (float32_t)sampleIndex;
        outputColor += gTexture.Sample(gSampler, texcoord);
    }

    outputColor *= rcp((float32_t)kNumSamples);

    PixelShaderOutput output;
    output.color.rgb = outputColor.rgb;
    output.color.a = 1.0f;
    return output;
}
