#include "BloomCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t3 baseColor = gTexture.Sample(gSampler, input.texcoord).rgb;
    float32_t3 bloom = gSecondaryTexture.Sample(gSampler, input.texcoord).rgb;

    float32_t3 color = baseColor + bloom * gBloom.intensity;
    color = (color - 0.5f) * 1.08f + 0.5f;
    color = ApplySaturation(color, 1.12f);
    color *= float32_t3(1.03f, 1.02f, 1.08f);

    float32_t2 centeredUv = input.texcoord - float32_t2(0.5f, 0.5f);
    float32_t edgeDistance = dot(centeredUv, centeredUv) * 2.0f;
    float32_t vignette = saturate(1.0f - edgeDistance * 0.42f);
    color *= lerp(1.0f, vignette, 0.48f);

    PixelShaderOutput output;
    output.color = float32_t4(saturate(color), 1.0f);
    return output;
}
