Texture2D gSkyTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    // テクスチャをそのままサンプリング
    return gSkyTexture.Sample(gSampler, input.texcoord);
}
