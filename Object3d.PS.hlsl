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
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float halfLambert = pow(NdotL * 0.5f + 0.5f, 2.0f);
        float3 litColor = gMaterialColor.rgb * gDirectionalLight.color.rgb * halfLambert;
        output.color = float4(litColor, 1.0f) * gTexture.Sample(gSampler, input.texcoord);

    }
    else
    {
        output.color = gMaterialColor * gTexture.Sample(gSampler, input.texcoord);
    }


    return output;
}
