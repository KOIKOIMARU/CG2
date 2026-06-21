#include "BloomCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t2 uv = input.texcoord;
    float32_t3 color =
        gTexture.Sample(gSampler, uv).rgb * 0.5f +
        gTexture.Sample(gSampler, uv + float32_t2( gBloom.texelSize.x, 0.0f)).rgb * 0.125f +
        gTexture.Sample(gSampler, uv + float32_t2(-gBloom.texelSize.x, 0.0f)).rgb * 0.125f +
        gTexture.Sample(gSampler, uv + float32_t2(0.0f,  gBloom.texelSize.y)).rgb * 0.125f +
        gTexture.Sample(gSampler, uv + float32_t2(0.0f, -gBloom.texelSize.y)).rgb * 0.125f;

    PixelShaderOutput output;
    output.color = float32_t4(ExtractBloom(color, gBloom.threshold), 1.0f);
    return output;
}
