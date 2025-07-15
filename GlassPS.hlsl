#include "Object3d.hlsli"

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor;
    int gEnableLighting;
    float crackAmount;
    float2 padding;
    float4x4 uvTransform;
};

float Hash21(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float GenerateCrack(float2 uv)
{
    float2 gridUV = uv * 30.0; // 細かさ
    float2 cell = floor(gridUV);
    float2 localUV = frac(gridUV);

    float minDist = 1.0;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float2 offset = float2(x, y);
            float2 randomOffset = float2(Hash21(cell + offset), Hash21(cell + offset + 5.17));
            float2 neighbor = offset + randomOffset - localUV;
            float dist = length(neighbor);
            minDist = min(minDist, dist);
        }
    }

    float crack = 1.0 - smoothstep(0.015, 0.035, minDist); // 細め
    return crack;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    
    float2 uv = mul(float4(input.texcoord, 0, 1), uvTransform).xy;

    float crack = GenerateCrack(uv) * crackAmount;

    // ★ デバッグカラー表示
    float3 color = lerp(float3(0.0, 0.0, 1.0), float3(1.0, 0.0, 0.0), crack); // blue→red

    
    return float4(color, 1.0f);
}
