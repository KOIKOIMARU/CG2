#include "BloomCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t2 stepUv = gBloom.texelSize * gBloom.direction;
    float32_t3 color = gTexture.Sample(gSampler, input.texcoord).rgb * 0.227027f;
    color += gTexture.Sample(gSampler, input.texcoord + stepUv * 1.384615f).rgb * 0.316216f;
    color += gTexture.Sample(gSampler, input.texcoord - stepUv * 1.384615f).rgb * 0.316216f;
    color += gTexture.Sample(gSampler, input.texcoord + stepUv * 3.230769f).rgb * 0.070270f;
    color += gTexture.Sample(gSampler, input.texcoord - stepUv * 3.230769f).rgb * 0.070270f;

    PixelShaderOutput output;
    output.color = float32_t4(color, 1.0f);
    return output;
}
