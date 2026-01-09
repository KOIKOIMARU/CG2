#include "Object3D.hlsli"

cbuffer MaterialCB : register(b0)
{
    Material gMaterial;
};
cbuffer CameraCB : register(b2)
{
    Camera gCamera;
};
cbuffer LightCB : register(b3)
{
    DirectionalLight gDirectionalLight;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    if (gMaterial.enableLighting != 0)
    {
        float3 N = normalize(input.normal);
        float3 L = normalize(-gDirectionalLight.direction.xyz);

        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        float NdotL = dot(N, L);
        float halfLambert = pow(NdotL * 0.5f + 0.5f, 2.0f);

// diffuse（今のままでOK）
        float3 diffuse =
    gMaterial.color.rgb * textureColor.rgb *
    gDirectionalLight.color.rgb * halfLambert *
    gDirectionalLight.intensity;

// ---- Blinn-Phong specular（ここが追加/変更）----
        float3 H = normalize(L + toEye); // HalfVector
        float NdotH = saturate(dot(N, H));
        float specularPow = pow(NdotH, gMaterial.shininess);

        float3 specular =
    gDirectionalLight.color.rgb *
    gDirectionalLight.intensity *
    specularPow;

        output.color.rgb = diffuse + specular;
        output.color.a = gMaterial.color.a * textureColor.a;

    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    return output;
}
