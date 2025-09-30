#include "Object3d.hlsli" // DirectionalLight 構造体を含む

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor;
    int gEnableLighting;
    float3 padding; // アライメント用
    float4x4 uvTransform;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer DirectionalLightCB : register(b3)
{
    DirectionalLight gDirectionalLight;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), uvTransform).xy;
    float4 tex = gTexture.Sample(gSampler, uv);

    // まずベース色とアルファを分離
    float3 baseRGB = gMaterialColor.rgb * tex.rgb;
    float outA = gMaterialColor.a * tex.a;

    if (gEnableLighting == 1)
    { // Lambert
        float3 N = normalize(input.normal);
        float3 L = normalize(-gDirectionalLight.direction);
        float NdotL = saturate(dot(N, L));

        float3 litRGB = baseRGB
                      * gDirectionalLight.color.rgb
                      * (gDirectionalLight.intensity * NdotL);

        output.color = float4(litRGB, outA);
    }
    else if (gEnableLighting == 2)
    { // Half-Lambert
        float3 N = normalize(input.normal);
        float3 L = normalize(-gDirectionalLight.direction);
        float halfLambert = dot(N, L) * 0.5f + 0.5f;
        float term = halfLambert * halfLambert; // 好みで二乗

        float3 litRGB = baseRGB
                      * gDirectionalLight.color.rgb
                      * (gDirectionalLight.intensity * term);

        output.color = float4(litRGB, outA);
    }
    else
    { // Unlit
        output.color = float4(baseRGB, outA);
    }
    return output;
}
