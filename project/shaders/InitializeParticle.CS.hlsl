// 全粒を未使用状態へ戻し、FreeListへすべてのスロット番号を登録する。
static const uint kMaxParticles = 1024;

struct Particle
{
    float3 translate;    // 現在のワールド位置
    float3 scale;        // 現在の描画サイズ
    float lifeTime;      // 生存する秒数
    float3 velocity;     // 1秒あたりの移動量
    float currentTime;   // 生成後の経過秒数
    float4 color;        // alphaが0なら未使用
    float3 initialScale; // 生成時のサイズ
    float endScale;      // 消滅時のサイズ倍率
};

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1); // FreeList末尾の有効番号
RWStructuredBuffer<uint> gFreeList : register(u2);     // 未使用粒子のスロット番号

[numthreads(1024, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    gParticles[particleIndex] = (Particle)0;
    gFreeList[particleIndex] = particleIndex;

    if (particleIndex == 0)
    {
        gFreeListIndex[0] = (int)kMaxParticles - 1;
    }
}
