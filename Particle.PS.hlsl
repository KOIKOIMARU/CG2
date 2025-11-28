#include "Particle.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    float4 tex = gTexture.Sample(gSampler, input.texcoord);

    float3 litColor = tex.rgb * input.color.rgb;
    float alpha = tex.a * input.color.a;

    PixelShaderOutput output;
    output.color = float4(litColor, alpha);

    return output;
}
