#include "Skybox.hlsli"

TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // TextureCubeはfloat3の方向ベクトルでサンプリングする
    output.color = gTexture.Sample(gSampler, normalize(input.texcoord));

    return output;
}
