#include "Object3d.hlsli" // DirectionalLight 構造体を含む

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor;
    int gEnableLighting;
    float3 padding; // 16byte alignment
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

    if (gEnableLighting != 0)
    {
        float3 normal = normalize(input.normal);
        float3 lightDir = normalize(-gDirectionalLight.direction.xyz);
        float NdotL = saturate(dot(normal, lightDir));
        float3 litColor = gMaterialColor.rgb * gDirectionalLight.color.rgb * NdotL;
        float4 tex = gTexture.Sample(gSampler, input.texcoord);
        output.color = float4(litColor, 1.0f) * tex;
    }
    else
    {
        output.color = gMaterialColor * gTexture.Sample(gSampler, input.texcoord);
    }


    return output;
}
