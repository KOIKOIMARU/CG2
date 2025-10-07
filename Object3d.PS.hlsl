#include "Object3d.hlsli" // DirectionalLight 構造体など

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor; // color.a はマテリアルα
    int gEnableLighting; // 0:Unlit 1:Lambert 2:HalfLambert
    float3 padding; // ← padding.x を alphaCutoff として使う
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

    // ベース色とα
    float3 baseRGB = gMaterialColor.rgb * tex.rgb;
    float outA = gMaterialColor.a * tex.a;

    // ★ αカット（padding.x をしきい値として使う）
    float alphaCutoff = padding.x; // 例: 0.1f
    if (outA <= alphaCutoff)
        discard; // 深度/色ともに書かない

    if (gEnableLighting == 1)           // Lambert
    {
        float3 N = normalize(input.normal);
        float3 L = normalize(-gDirectionalLight.direction);
        float NdotL = saturate(dot(N, L));

        float3 litRGB = baseRGB *
                        gDirectionalLight.color.rgb *
                        (gDirectionalLight.intensity * NdotL);

        output.color = float4(litRGB, outA);
    }
    else if (gEnableLighting == 2)      // Half-Lambert
    {
        float3 N = normalize(input.normal);
        float3 L = normalize(-gDirectionalLight.direction);
        float halfLambert = dot(N, L) * 0.5f + 0.5f;
        float term = halfLambert * halfLambert;

        float3 litRGB = baseRGB *
                        gDirectionalLight.color.rgb *
                        (gDirectionalLight.intensity * term);

        output.color = float4(litRGB, outA);
    }
    else // Unlit
    {
        output.color = float4(baseRGB, outA);
    }
    return output;
}
