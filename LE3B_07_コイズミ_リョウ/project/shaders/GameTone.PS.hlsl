#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer GameToneParameter : register(b0)
{
    float32_t vignetteStrength;
    float32_t saturation;
    float32_t contrast;
    float32_t damageTint;
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

static const float32_t kPI = 3.14159265f;

static const float32_t2 kIndex3x3[3][3] = {
    { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
    { { -1.0f,  0.0f }, { 0.0f,  0.0f }, { 1.0f,  0.0f } },
    { { -1.0f,  1.0f }, { 0.0f,  1.0f }, { 1.0f,  1.0f } },
};

float32_t Gauss(float32_t x, float32_t y, float32_t sigma) {
    float32_t exponent = -(x * x + y * y) * rcp(2.0f * sigma * sigma);
    float32_t denominator = 2.0f * kPI * sigma * sigma;
    return exp(exponent) * rcp(denominator);
}

float32_t3 ApplySaturation(float32_t3 color, float32_t amount) {
    float32_t luma = dot(color, float32_t3(0.299f, 0.587f, 0.114f));
    return lerp(float32_t3(luma, luma, luma), color, amount);
}

PixelShaderOutput main(VertexShaderOutput input) {
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(width), rcp(height));

    float32_t weight = 0.0f;
    float32_t kernel3x3[3][3];
    for (int32_t x = 0; x < 3; ++x) {
        for (int32_t y = 0; y < 3; ++y) {
            kernel3x3[x][y] =
                Gauss(kIndex3x3[x][y].x, kIndex3x3[x][y].y, 1.6f);
            weight += kernel3x3[x][y];
        }
    }

    float32_t3 softColor = float32_t3(0.0f, 0.0f, 0.0f);
    for (int32_t sx = 0; sx < 3; ++sx) {
        for (int32_t sy = 0; sy < 3; ++sy) {
            float32_t2 texcoord =
                input.texcoord + kIndex3x3[sx][sy] * uvStepSize;
            softColor +=
                gTexture.Sample(gSampler, texcoord).rgb * kernel3x3[sx][sy];
        }
    }
    softColor *= rcp(weight);

    float32_t3 baseColor = gTexture.Sample(gSampler, input.texcoord).rgb;
    float32_t3 color = lerp(baseColor, softColor, 0.28f);
    color += (baseColor - softColor) * 0.18f;

    color = (color - 0.5f) * contrast + 0.5f;
    color = ApplySaturation(color, saturation);
    color *= float32_t3(1.03f, 1.02f, 1.08f);

    float32_t2 centeredUv = input.texcoord - float32_t2(0.5f, 0.5f);
    float32_t edgeDistance = dot(centeredUv, centeredUv) * 2.0f;
    float32_t vignette = saturate(1.0f - edgeDistance * vignetteStrength);
    color *= lerp(1.0f, vignette, 0.72f);

    float32_t3 damageColor = float32_t3(1.0f, 0.22f, 0.16f);
    color = lerp(color, color * damageColor, damageTint);

    PixelShaderOutput output;
    output.color = float32_t4(saturate(color), 1.0f);
    return output;
}
