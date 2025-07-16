#include "Object3d.hlsli"

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor;
    int gEnableLighting;
    float crackAmount;
    float2 padding;
    float4x4 uvTransform;
};

// 2Dハッシュ関数
float Hash21(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

// ひび割れ生成関数
float GenerateCrack(float2 uv)
{
    float2 gridUV = uv * 30.0; // 密度（大きいほど細かく）
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

    // スムースステップで線の太さ調整（調整済み）
    float crack = 1.0 - smoothstep(0.005, 0.08, minDist);
    return crack;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    // UV変換（回転やスケールを ImGui で調整可能）
    float2 uv = mul(float4(input.texcoord, 0, 1), uvTransform).xy;

    // 割れ量（ImGuiから制御可能）
    float crack = GenerateCrack(uv) * crackAmount;

    // ★ 色付けバリエーション（必要に応じて切り替えてください）

    // グレースケール表示（割れのみ確認）
    // float3 color = crack.xxx;

    // 青→赤のデバッグカラー
    float3 color = lerp(float3(0.0, 0.0, 1.0), float3(1.0, 0.0, 0.0), crack);

    // 氷のような色味（淡い青）
    // float3 color = lerp(float3(0.4, 0.7, 1.0), float3(1.0, 1.0, 1.0), crack);

    // 透明感＋割れを強調したい場合 → alpha = crack などもあり
    return float4(color, 1.0f);
}
