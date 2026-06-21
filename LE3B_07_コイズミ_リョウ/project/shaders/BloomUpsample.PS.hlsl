#include "BloomCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t3 fineBloom = gTexture.Sample(gSampler, input.texcoord).rgb;
    float32_t3 wideBloom = gSecondaryTexture.Sample(gSampler, input.texcoord).rgb;

    PixelShaderOutput output;
    output.color = float32_t4(fineBloom + wideBloom * gBloom.scatter, 1.0f);
    return output;
}
