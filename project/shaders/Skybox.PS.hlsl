#include "Skybox.hlsli"

// 頂点方向に対応するキューブマップの色を背景として出力する。
TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    output.color = gTexture.Sample(gSampler, normalize(input.texcoord));

    return output;
}
