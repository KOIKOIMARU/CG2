// 生存粒の位置・透明度・大きさを進め、寿命切れのスロットをFreeListへ返す。
static const uint kMaxParticles = 1024;

struct Particle
{
    float3 translate;    // 現在のワールド位置
    float3 scale;        // 現在の描画サイズ
    float lifeTime;      // 生存する秒数
    float3 velocity;     // 1秒あたりの移動量
    float currentTime;   // 生成後の経過秒数
    float4 color;        // alphaが0より大きい間だけ生存
    float3 initialScale; // 生成時のサイズ
    float endScale;      // 消滅時のサイズ倍率
};

struct PerFrame
{
    float time;        // 初期化後の累積秒数
    float deltaTime;   // 今フレームの経過秒数
    float2 padding;    // 定数バッファ境界用の余白
};

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1); // FreeList末尾の有効番号
RWStructuredBuffer<uint> gFreeList : register(u2);     // 未使用粒子のスロット番号
ConstantBuffer<PerFrame> gPerFrame : register(b0);

[numthreads(1024, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    if (gParticles[particleIndex].color.a != 0.0f)
    {
        gParticles[particleIndex].translate +=
            gParticles[particleIndex].velocity * gPerFrame.deltaTime;
        gParticles[particleIndex].currentTime += gPerFrame.deltaTime;

        float alpha =
            1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
        gParticles[particleIndex].color.a = saturate(alpha);
        float lifeRate = saturate(
            gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
        float smoothLifeRate = lifeRate * lifeRate * (3.0f - 2.0f * lifeRate);
        gParticles[particleIndex].scale = lerp(
            gParticles[particleIndex].initialScale,
            gParticles[particleIndex].initialScale * gParticles[particleIndex].endScale,
            smoothLifeRate);

        if (gParticles[particleIndex].color.a == 0.0f)
        {
            gParticles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);

            int freeListIndex;
            InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);

            if ((freeListIndex + 1) < kMaxParticles)
            {
                gFreeList[freeListIndex + 1] = particleIndex;
            }
            else
            {
                // 二重返却などで上限を超えた場合は、先に増やしたカウンターを元へ戻す。
                int unused;
                InterlockedAdd(gFreeListIndex[0], -1, unused);
            }
        }
    }
}
