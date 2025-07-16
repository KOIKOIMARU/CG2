#include "Object3d.hlsli"

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor;
    int gEnableLighting;
    float crackAmount;
    float2 padding;
    float4x4 uvTransform;
}

cbuffer LightCB : register(b3)
{
    float4 gLightColor;
    float3 gLightDirection;
    float gLightIntensity;
}

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
    float3 worldPos : TEXCOORD3;
};

float4 main(PixelInput input) : SV_TARGET
{
    float2 uv = mul(float4(input.texcoord, 0, 1), uvTransform).xy;
    float4 texColor = gTexture.Sample(gSampler, uv);

    // 基本色（青っぽく、透明度あり）
    float3 baseColor = float3(0.2, 0.5, 0.8); // 青系
    float alpha = 0.3f; // 透明度

    // ライティング（鏡面反射を追加）
    if (gEnableLighting != 0)
    {
        float3 normal = normalize(input.worldNormal);
        float3 lightDir = normalize(-gLightDirection);
        float3 viewDir = normalize(-input.worldPos); // カメラ方向

        // 拡散光
        float diff = saturate(dot(normal, lightDir));

        // 鏡面反射（ハイライト）
        float3 halfVec = normalize(lightDir + viewDir);
        float spec = pow(saturate(dot(normal, halfVec)), 100.0f); // 鋭いハイライト

        baseColor *= (0.2 + diff * 0.5); // 拡散光で少し明るく
        baseColor += spec * gLightColor.rgb * 2.0; // ハイライトを加算
    }

    return float4(baseColor, alpha);
}
