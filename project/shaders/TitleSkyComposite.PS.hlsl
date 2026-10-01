#include "Fullscreen.hlsli"

Texture2D<float4> cloudTexture : register(t0);
SamplerState cloudSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint width, height;
    cloudTexture.GetDimensions(width, height);
    float2 texel = 1.0f / float2(width, height);
    // 密度積分の微小な段差を背景だけでならす。機体とロゴには適用しない。
    float3 color = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float weight = (x == 0 ? 2.0f : 1.0f) * (y == 0 ? 2.0f : 1.0f);
            color += cloudTexture.Sample(cloudSampler, input.texcoord + float2(x,y) * texel).rgb * weight;
        }
    }
    return float4(color * (1.0f / 16.0f), 1.0f);
}
