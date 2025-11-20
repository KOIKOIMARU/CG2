#include "Object3d.hlsli" 

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor;
    int gEnableLighting;
    float3 padding; 
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
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(-gDirectionalLight.direction);

    if (gEnableLighting == 1)
    { // Lambert
        float NdotL = saturate(dot(normal, lightDir));
        float3 litColor = gMaterialColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * NdotL;
        output.color = float4(litColor, 1.0f) * tex;
    }
    else if (gEnableLighting == 2)
    { // Half Lambert
        float NdotL = dot(normal, lightDir);
        float halfLambert = NdotL * 0.5 + 0.5;
        float3 litColor = gMaterialColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * halfLambert * halfLambert;
        output.color = float4(litColor, 1.0f) * tex;
    }
    else
    {
        output.color = gMaterialColor * tex;
    }

    return output;
}

