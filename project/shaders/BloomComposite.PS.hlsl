#include "BloomCommon.hlsli"

// 元画像へブルームを合成し、フィルミックカーブと軽い色調補正で仕上げる。
PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t3 baseColor = gTexture.Sample(gSampler, input.texcoord).rgb;
    float32_t3 bloom = gSecondaryTexture.Sample(gSampler, input.texcoord).rgb;

    float32_t bloomLuminance = Luminance(bloom);
    float32_t bloomGate = saturate((bloomLuminance - 0.015f) * 18.0f);
    float32_t3 color =
        baseColor +
        bloom * gBloom.intensity * bloomGate;
    color +=
        bloom *
        saturate(bloomLuminance * 1.45f) *
        gBloom.intensity *
        0.18f;

    float32_t3 filmicColor = ApplyFilmicCurve(color * 1.10f) * 1.06f;
    color = lerp(color, filmicColor, 0.42f);
    color = (color - 0.5f) * 1.055f + 0.5f;
    color = ApplySaturation(color, 1.075f);
    color *= float32_t3(1.025f, 1.015f, 1.045f);

    float32_t2 centeredUv = input.texcoord - float32_t2(0.5f, 0.5f);
    float32_t edgeDistance = dot(centeredUv, centeredUv) * 2.0f;
    float32_t vignette = saturate(1.0f - edgeDistance * 0.40f);
    color *= lerp(1.0f, vignette, 0.42f);

    PixelShaderOutput output;
    output.color = float32_t4(saturate(color), 1.0f);
    return output;
}
