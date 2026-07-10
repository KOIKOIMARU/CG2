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
    float32_t exposure;
    float32_t blackPoint;
    float32_t highlightCompression;
    float32_t colorTemperature;
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

float32_t3 ApplySaturation(float32_t3 color, float32_t amount) {
    float32_t luma = dot(color, float32_t3(0.299f, 0.587f, 0.114f));
    return lerp(float32_t3(luma, luma, luma), color, amount);
}

float32_t GetLuminance(float32_t3 color) {
    return dot(color, float32_t3(0.2126f, 0.7152f, 0.0722f));
}

float32_t3 SampleSceneColor(float32_t2 texcoord) {
    return gTexture.Sample(gSamplerLinear, saturate(texcoord)).rgb;
}

float32_t3 ApplyEdgeAntialias(
    float32_t2 texcoord,
    float32_t2 texelSize,
    float32_t3 sourceColor) {
    float32_t lumaCenter = GetLuminance(sourceColor);
    float32_t3 left =
        SampleSceneColor(texcoord - float32_t2(texelSize.x, 0.0f));
    float32_t3 right =
        SampleSceneColor(texcoord + float32_t2(texelSize.x, 0.0f));
    float32_t3 up =
        SampleSceneColor(texcoord - float32_t2(0.0f, texelSize.y));
    float32_t3 down =
        SampleSceneColor(texcoord + float32_t2(0.0f, texelSize.y));

    float32_t lumaLeft = GetLuminance(left);
    float32_t lumaRight = GetLuminance(right);
    float32_t lumaUp = GetLuminance(up);
    float32_t lumaDown = GetLuminance(down);
    float32_t lumaMin =
        min(lumaCenter, min(min(lumaLeft, lumaRight), min(lumaUp, lumaDown)));
    float32_t lumaMax =
        max(lumaCenter, max(max(lumaLeft, lumaRight), max(lumaUp, lumaDown)));
    float32_t lumaRange = lumaMax - lumaMin;
    float32_t contrastThreshold = max(0.045f, lumaMax * 0.135f);
    if (lumaRange < contrastThreshold) {
        return sourceColor;
    }

    float32_t horizontalGradient = abs(lumaLeft - lumaRight);
    float32_t verticalGradient = abs(lumaUp - lumaDown);
    float32_t3 antialiasedColor =
        horizontalGradient > verticalGradient ?
        sourceColor * 0.62f + (up + down) * 0.19f :
        sourceColor * 0.62f + (left + right) * 0.19f;
    float32_t edgeStrength =
        saturate((lumaRange - contrastThreshold) * 2.8f) * 0.46f;
    return lerp(sourceColor, antialiasedColor, edgeStrength);
}

float32_t3 ApplyDistantShimmerReduction(
    float32_t2 texcoord,
    float32_t2 texelSize,
    float32_t3 sourceColor,
    float32_t viewDepth,
    float32_t skyMask)
{
    float32_t distanceFactor =
        smoothstep(135.0f, 320.0f, viewDepth) * (1.0f - skyMask);
    if (distanceFactor <= 0.001f) {
        return sourceColor;
    }

    float32_t2 offset = float32_t2(0.0f, texelSize.y * 1.25f);
    float32_t3 calmColor =
        sourceColor * 0.62f +
        (SampleSceneColor(texcoord + offset) +
         SampleSceneColor(texcoord - offset)) *
        0.19f;

    float32_t centerLuma = GetLuminance(sourceColor);
    float32_t calmLuma = GetLuminance(calmColor);
    float32_t highFrequency =
        saturate(abs(centerLuma - calmLuma) * 6.0f);
    return lerp(sourceColor, calmColor, distanceFactor * highFrequency * 0.26f);
}

float32_t3 ApplyFilmicCurve(float32_t3 color) {
    color = max(color, float32_t3(0.0f, 0.0f, 0.0f));
    return saturate(
        (color * (2.51f * color + 0.03f)) /
        (color * (2.43f * color + 0.59f) + 0.14f));
}

float32_t3 ApplyColorTemperature(float32_t3 color, float32_t temperature)
{
    float32_t t = clamp(temperature, -1.0f, 1.0f);
    float32_t3 balance = float32_t3(
        1.0f + t * 0.055f,
        1.0f + abs(t) * 0.010f,
        1.0f - t * 0.065f);
    return color * balance;
}

float32_t3 ApplyCinematicGrade(
    float32_t3 color,
    float32_t viewDepth,
    float32_t skyMask)
{
    color *= exposure;
    color = max(color - blackPoint, float32_t3(0.0f, 0.0f, 0.0f)) *
        rcp(max(1.0f - blackPoint, 0.001f));

    float32_t luminance = GetLuminance(color);
    float32_t shadowMask = pow(saturate(1.0f - luminance * 1.70f), 2.0f);
    float32_t midMask =
        saturate(1.0f - abs(luminance - 0.43f) * 2.35f);
    float32_t highlightMask = smoothstep(0.52f, 1.08f, luminance);

    color = lerp(
        color,
        color * float32_t3(0.88f, 0.95f, 1.06f),
        shadowMask * 0.12f * (1.0f - skyMask * 0.55f));
    color = lerp(
        color,
        color * float32_t3(1.035f, 1.018f, 0.970f),
        midMask * 0.055f);
    color = lerp(
        color,
        color * float32_t3(1.060f, 1.025f, 0.955f),
        highlightMask * 0.085f);

    float32_t distanceWash =
        smoothstep(125.0f, 320.0f, viewDepth) * (1.0f - skyMask);
    color = lerp(color, ApplySaturation(color, 0.84f), distanceWash * 0.22f);

    float32_t shoulder = saturate(highlightCompression);
    float32_t3 compressed =
        color * rcp(1.0f + color * (0.42f + shoulder * 0.46f));
    color = lerp(color, compressed * 1.18f, shoulder * 0.36f);
    return ApplyColorTemperature(color, colorTemperature);
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

float32_t CalculateScreenSpaceAO(
    float32_t2 texcoord,
    float32_t2 texelSize,
    float32_t viewDepth,
    float32_t ndcDepth) {
    if (ndcDepth >= 0.9999f) {
        return 1.0f;
    }

    static const float32_t2 aoOffsets[3] = {
        float32_t2(-1.0f,  0.0f),
        float32_t2( 1.0f,  0.0f),
        float32_t2( 0.0f,  1.0f),
    };

    float32_t sampleRadius =
        lerp(1.8f, 4.6f, saturate(viewDepth / 150.0f));
    float32_t depthRange = 0.92f + viewDepth * 0.018f;
    float32_t occlusion = 0.0f;
    float32_t validSamples = 0.0f;

    [unroll]
    for (int32_t index = 0; index < 3; ++index) {
        float32_t2 sampleUv =
            saturate(texcoord + aoOffsets[index] * texelSize * sampleRadius);
        float32_t sampleNdcDepth =
            gDepthTexture.Sample(gSamplerPoint, sampleUv);
        if (sampleNdcDepth >= 0.9999f) {
            continue;
        }

        float32_t sampleDepth = FetchViewDepth(sampleUv);
        float32_t closerDepth = viewDepth - sampleDepth;
        float32_t localOcclusion =
            smoothstep(0.18f, depthRange, closerDepth);
        float32_t rangeFade =
            saturate(1.0f - abs(closerDepth) / max(depthRange * 4.0f, 0.01f));
        occlusion += localOcclusion * rangeFade;
        validSamples += 1.0f;
    }

    if (validSamples <= 0.5f) {
        return 1.0f;
    }

    float32_t nearFade = 1.0f - smoothstep(160.0f, 285.0f, viewDepth);
    float32_t aoStrength = 0.105f * nearFade;
    return 1.0f - saturate(occlusion / validSamples) * aoStrength;
}

PixelShaderOutput main(VertexShaderOutput input) {
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(width), rcp(height));

    float32_t3 baseColor =
        gTexture.Sample(gSamplerLinear, input.texcoord).rgb;
    float32_t3 softColor =
        baseColor * 0.58f +
        (
            SampleSceneColor(input.texcoord + float32_t2(uvStepSize.x, 0.0f)) +
            SampleSceneColor(input.texcoord - float32_t2(uvStepSize.x, 0.0f)) +
            SampleSceneColor(input.texcoord + float32_t2(0.0f, uvStepSize.y)) +
            SampleSceneColor(input.texcoord - float32_t2(0.0f, uvStepSize.y))
        ) *
        0.105f;
    float32_t viewDepth = FetchViewDepth(input.texcoord);
    float32_t ndcDepth =
        gDepthTexture.Sample(gSamplerPoint, input.texcoord);
    float32_t skyMask = step(0.9999f, ndcDepth);
    baseColor = ApplyEdgeAntialias(input.texcoord, uvStepSize, baseColor);
    baseColor = ApplyDistantShimmerReduction(
        input.texcoord,
        uvStepSize,
        baseColor,
        viewDepth,
        skyMask);
    float32_t3 color = lerp(baseColor, softColor, 0.050f);
    color += (baseColor - softColor) * 0.12f;

    color = (color - 0.5f) * contrast + 0.5f;
    color = ApplySaturation(color, saturation);
    color *= float32_t3(1.030f, 1.018f, 1.030f);

    float32_t luminance = GetLuminance(softColor);
    float32_t softGlow = saturate((luminance - 0.60f) * 1.85f);
    color += softColor * softGlow * 0.035f;

    float32_t aoFactor = CalculateScreenSpaceAO(
        input.texcoord,
        uvStepSize,
        viewDepth,
        ndcDepth);
    color *= aoFactor;

    float32_t farStability = saturate((viewDepth - 96.0f) / 180.0f);
    color = lerp(color, softColor, farStability * 0.015f);
    float32_t fogRange = max(fogEnd - fogStart, 1.0f);
    float32_t depthFog = smoothstep(
        0.0f,
        1.0f,
        saturate((viewDepth - fogStart) / fogRange));
    float32_t horizonFog = pow(
        saturate(1.0f - abs(input.texcoord.y - 0.48f) * 3.4f),
        1.55f);
    float32_t fogAmount =
        depthFog * fogStrength +
        horizonFog * horizonFogStrength * (0.35f + depthFog * 0.65f);
    fogAmount *= lerp(1.0f, 0.22f, skyMask);

    float32_t3 fogColor = lerp(
        float32_t3(0.52f, 0.62f, 0.74f),
        float32_t3(0.78f, 0.86f, 0.95f),
        saturate(input.texcoord.y * 1.12f));
    color = lerp(color, fogColor, saturate(fogAmount));

    float32_t sunGlow = pow(
        saturate(
            1.0f -
            distance(input.texcoord, float32_t2(0.58f, 0.38f)) * 1.85f),
        4.0f);
    color += sunGlow * 0.020f * float32_t3(1.0f, 0.86f, 0.62f);
    color = ApplyCinematicGrade(color, viewDepth, skyMask);
    float32_t filmicAmount = 0.25f + saturate(highlightCompression) * 0.12f;
    color = lerp(color, ApplyFilmicCurve(color) * 1.07f, filmicAmount);

    float32_t2 centeredUv = input.texcoord - float32_t2(0.5f, 0.5f);
    float32_t edgeDistance = dot(centeredUv, centeredUv) * 2.0f;
    float32_t vignette = saturate(1.0f - edgeDistance * vignetteStrength);
    color *= lerp(1.0f, vignette, 0.66f);

    float32_t3 damageColor = float32_t3(1.0f, 0.22f, 0.16f);
    color = lerp(color, color * damageColor, damageTint);

    PixelShaderOutput output;
    output.color = float32_t4(saturate(color), 1.0f);
    return output;
}
