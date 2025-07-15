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
    uv += input.normal.xy * 0.04;

    float4 tex = gTexture.Sample(gSampler, uv);
    float3 normal = normalize(input.worldNormal);
    float3 viewDir = normalize(-input.worldPos);

    float fresnel = pow(1.0 - saturate(dot(viewDir, normal)), 5.0);
    float rim = pow(1.0 - saturate(dot(viewDir, normal)), 2.0);

    float alpha = 0.12; // 透明感UP
    float3 crystalColor = float3(0.4, 0.8, 1.4); // より青く・強く

    float3 baseColor = tex.rgb * crystalColor;
    float3 finalColor = baseColor;

    if (gEnableLighting != 0)
    {
        float3 lightDir = normalize(-gDirectionalLight.direction.xyz);
        float NdotL = saturate(dot(normal, lightDir));
        float3 diffuse = baseColor * gDirectionalLight.color.rgb * NdotL;

        float3 halfDir = normalize(lightDir + viewDir);
        float spec = pow(saturate(dot(normal, halfDir)), 64.0);

        // より強いFresnelとリムでEmissive風に
        float3 emissive = (fresnel + rim) * float3(0.6, 1.0, 1.8);
        finalColor = diffuse + spec * 0.4 + emissive;
    }
    else
    {
        finalColor = baseColor + (fresnel + rim) * float3(0.6, 1.0, 1.8);
    }

    return float4(saturate(finalColor), alpha);
}
