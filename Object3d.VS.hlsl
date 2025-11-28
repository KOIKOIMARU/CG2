cbuffer TransformCB : register(b1)
{
    float4x4 gWVP;
    float4x4 gWorld;
};

// ★ ここを Instancing 対応に書き換える！
struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;

    // ▼ 追加：インスタンスワールド行列
    float4 i0 : INSTANCE0;
    float4 i1 : INSTANCE1;
    float4 i2 : INSTANCE2;
    float4 i3 : INSTANCE3;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // ★ インスタンスごとのワールド行列を合成
    float4x4 instanceWorld = float4x4(input.i0, input.i1, input.i2, input.i3);

    // ★ 座標変換（instanceWorld → gWVP の順で掛ける）
    float4 worldPos = mul(input.position, instanceWorld);
    output.position = mul(worldPos, gWVP);

    output.texcoord = input.texcoord;

    // 法線も instanceWorld を使う
    float3x3 normalMatrix = (float3x3) instanceWorld;
    output.normal = normalize(mul(input.normal, normalMatrix));

    return output;
}
