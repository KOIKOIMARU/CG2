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

    float2 uv;
    if (gEnableLighting == 0)
    {
        float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), uvTransform);
        uv = transformedUV.xy;
    }
    else
    {
        uv = input.texcoord;
    }

    float4 tex = gTexture.Sample(gSampler, uv);

    if (gEnableLighting == 1)
    { // Lambert
        float3 normal = normalize(input.normal);
        float3 lightDir = normalize(-gDirectionalLight.direction.xyz);
        float NdotL = saturate(dot(normal, lightDir));
        float3 litColor = gMaterialColor.rgb * gDirectionalLight.color.rgb * NdotL ;
        output.color = float4(litColor, 1.0f) * tex;
    }
    else if (gEnableLighting == 2)
    { // Half Lambert
        float3 normal = normalize(input.normal);
        float3 lightDir = normalize(-gDirectionalLight.direction.xyz);
        float NdotL = dot(normal, lightDir);
        float halfLambert = NdotL * 0.5 + 0.5;
        float3 litColor = gMaterialColor.rgb * gDirectionalLight.color.rgb * halfLambert * halfLambert ;
        output.color = float4(litColor, 1.0f) * tex;
    }
    else // Lightingなし
    {
        output.color = gMaterialColor * tex;
    }

    return output;
}
