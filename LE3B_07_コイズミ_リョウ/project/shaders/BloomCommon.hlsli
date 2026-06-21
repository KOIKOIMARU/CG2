#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t4> gSecondaryTexture : register(t1);
SamplerState gSampler : register(s0);

struct BloomParameter {
    float32_t2 texelSize;
    float32_t2 direction;
    float32_t threshold;
    float32_t intensity;
    float32_t scatter;
    float32_t padding;
};

ConstantBuffer<BloomParameter> gBloom : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

float32_t Luminance(float32_t3 color)
{
    return dot(color, float32_t3(0.2126f, 0.7152f, 0.0722f));
}

float32_t3 ApplySaturation(float32_t3 color, float32_t amount)
{
    float32_t luma = dot(color, float32_t3(0.299f, 0.587f, 0.114f));
    return lerp(float32_t3(luma, luma, luma), color, amount);
}

float32_t3 ExtractBloom(float32_t3 color, float32_t threshold)
{
    float32_t luminance = Luminance(color);
    float32_t mask = smoothstep(threshold, threshold + 0.55f, luminance);
    return color * mask;
}
