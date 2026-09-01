// GPU上の粒状態から、カメラ正面を向く板ポリゴンの頂点を組み立てる。
struct VertexIn
{
    float4 position : POSITION; // 全粒で共有する板のローカル座標
    float2 texcoord : TEXCOORD; // 粒テクスチャのUV座標
};

struct VertexOut
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR0;
};

struct Particle
{
    float3 translate;    // 現在のワールド位置
    float3 scale;        // 現在の描画サイズ
    float lifeTime;      // 生存する秒数
    float3 velocity;     // 1秒あたりの移動量
    float currentTime;   // 生成後の経過秒数
    float4 color;        // 描画するRGBA色
    float3 initialScale; // 生成時のサイズ
    float endScale;      // 消滅時のサイズ倍率
};

struct PerView
{
    float4x4 viewProjection; // ワールド座標からクリップ座標への変換
    float4x4 billboardMatrix; // 板をカメラ正面へ向ける回転
};

StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

VertexOut main(VertexIn input, uint instanceId : SV_InstanceID)
{
    VertexOut output;
    Particle particle = gParticles[instanceId];

    float4x4 worldMatrix = gPerView.billboardMatrix;
    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;
    worldMatrix[3].xyz = particle.translate;

    output.position = mul(input.position, mul(worldMatrix, gPerView.viewProjection));
    output.texcoord = input.texcoord;
    output.color = particle.color;
    return output;
}
