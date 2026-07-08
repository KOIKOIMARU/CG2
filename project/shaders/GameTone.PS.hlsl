#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gDepthTexture : register(t1);
SamplerState gSamplerLinear : register(s0);
SamplerState gSamplerPoint : register(s1);

cbuffer GameToneParameter : register(b0)
{
    float32_t4x4 gProjectionInverse;
    float32_t vignetteStrength;
    float32_t saturation;
    float32_t contrast;
    float32_t damageTint;
    float32_t fogStart;
    float32_t fogEnd;
    float32_t fogStrength;
    float32_t horizonFogStrength;
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

float32_t3 ApplyFilmicCurve(float32_t3 color) {
    color = max(color, float32_t3(0.0f, 0.0f, 0.0f));
    return saturate(
        (color * (2.51f * color + 0.03f)) /
        (color * (2.43f * color + 0.59f) + 0.14f));
}

float32_t FetchViewDepth(float32_t2 texcoord) {
    float32_t ndcDepth = gDepthTexture.Sample(gSamplerPoint, texcoord);
    float32_t4 ndcPosition = float32_t4(
        texcoord.x * 2.0f - 1.0f,
        1.0f - texcoord.y * 2.0f,
        ndcDepth,
        1.0f);

    float32_t4 viewPosition = mul(ndcPosition, gProjectionInverse);
    float32_t safeW =
        abs(viewPosition.w) < 0.00001f ? 0.00001f : viewPosition.w;
    viewPosition.xyz /= safeW;
    return abs(viewPosition.z);
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
                gTexture.Sample(gSamplerLinear, texcoord).rgb *
                kernel3x3[sx][sy];
        }
    }
    softColor *= rcp(weight);

    float32_t3 baseColor =
        gTexture.Sample(gSamplerLinear, input.texcoord).rgb;
    float32_t3 color = lerp(baseColor, softColor, 0.16f);
    color += (baseColor - softColor) * 0.10f;

    color = (color - 0.5f) * contrast + 0.5f;
    color = ApplySaturation(color, saturation);
    color *= float32_t3(1.04f, 1.02f, 1.08f);

    float32_t luminance =
        dot(softColor, float32_t3(0.2126f, 0.7152f, 0.0722f));
    float32_t softGlow = saturate((luminance - 0.62f) * 2.35f);
    color += softColor * softGlow * 0.11f;

    float32_t viewDepth = FetchViewDepth(input.texcoord);
    float32_t ndcDepth =
        gDepthTexture.Sample(gSamplerPoint, input.texcoord);
    float32_t fogRange = max(fogEnd - fogStart, 1.0f);
    float32_t depthFog = smoothstep(
        0.0f,
        1.0f,
        saturate((viewDepth - fogStart) / fogRange));
    float32_t horizonFog = pow(
        saturate(1.0f - abs(input.texcoord.y - 0.48f) * 3.4f),
        1.55f);
    float32_t skyMask = step(0.9999f, ndcDepth);
    float32_t fogAmount =
        depthFog * fogStrength +
        horizonFog * horizonFogStrength * (0.35f + depthFog * 0.65f);
    fogAmount *= lerp(1.0f, 0.28f, skyMask);

    float32_t3 fogColor = lerp(
        float32_t3(0.58f, 0.66f, 0.77f),
        float32_t3(0.78f, 0.85f, 0.94f),
        saturate(input.texcoord.y * 1.12f));
    color = lerp(color, fogColor, saturate(fogAmount));

    float32_t sunGlow = pow(
        saturate(
            1.0f -
            distance(input.texcoord, float32_t2(0.58f, 0.38f)) * 1.85f),
        4.0f);
    color += sunGlow * 0.035f * float32_t3(1.0f, 0.86f, 0.62f);
    color = lerp(color, ApplyFilmicCurve(color) * 1.08f, 0.38f);

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
