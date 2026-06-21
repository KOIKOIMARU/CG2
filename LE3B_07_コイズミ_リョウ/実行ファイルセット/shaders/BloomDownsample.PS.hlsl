#include "BloomCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t2 texel = gBloom.texelSize;
    float32_t2 uv = input.texcoord;
    float32_t3 color =
        gTexture.Sample(gSampler, uv).rgb * 0.40f +
        gTexture.Sample(gSampler, uv + texel * float32_t2( 1.0f,  1.0f)).rgb * 0.15f +
        gTexture.Sample(gSampler, uv + texel * float32_t2(-1.0f,  1.0f)).rgb * 0.15f +
        gTexture.Sample(gSampler, uv + texel * float32_t2( 1.0f, -1.0f)).rgb * 0.15f +
        gTexture.Sample(gSampler, uv + texel * float32_t2(-1.0f, -1.0f)).rgb * 0.15f;

    PixelShaderOutput output;
    output.color = float32_t4(color, 1.0f);
    return output;
}
