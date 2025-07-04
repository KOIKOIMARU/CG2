#include "Object3d.hlsli"

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor; // rgba
    int gEnableLighting;
    float3 padding;
    float4x4 uvTransform;
};

cbuffer DirectionalLightCB : register(b3)
{
    DirectionalLight gDirectionalLight;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float Fresnel(float3 viewDir, float3 normal, float power)
{
    return pow(1.0 - saturate(dot(viewDir, normal)), power);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = mul(float4(input.texcoord, 0, 1), uvTransform).xy;
    float4 tex = gTexture.Sample(gSampler, uv);

    float3 normal = normalize(input.worldNormal);
    float3 viewDir = normalize(-input.worldPos); // カメラが原点にある前提

    float fresnel = Fresnel(viewDir, normal, 5.0); // 簡易的な光沢

    float alpha = gMaterialColor.a;
    float3 baseColor = tex.rgb * gMaterialColor.rgb;
    float3 finalColor = baseColor;

    if (gEnableLighting != 0)
    {
        float3 lightDir = normalize(-gDirectionalLight.direction.xyz);
        float NdotL = saturate(dot(normal, lightDir));
        float3 lit = baseColor * gDirectionalLight.color.rgb * NdotL;
        finalColor = lerp(lit, lit + fresnel, 0.3); // フレネル反射を少し追加
    }
    else
    {
        finalColor = baseColor + fresnel * 0.2;
    }

    return float4(finalColor, alpha);
}
