// FreeListから空きスロットを取得し、Emitter条件に従って新しい粒を生成する。
static const uint kMaxParticles = 1024;

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

struct EmitterSphere
{
    float3 translate;  // ワールド空間の発生中心
    float radius;      // 球状発生範囲の半径
    float3 direction;  // 飛翔の基準方向
    float spread;      // 基準方向からのばらつき
    float4 colorMin;   // 生成色の乱数下限
    float4 colorMax;   // 生成色の乱数上限
    float2 scaleMin;   // 生成サイズの乱数下限
    float2 scaleMax;   // 生成サイズの乱数上限
    float lifeTimeMin; // 寿命の乱数下限
    float lifeTimeMax; // 寿命の乱数上限
    float speedMin;    // 初速の乱数下限
    float speedMax;    // 初速の乱数上限
    float endScale;    // 消滅時のサイズ倍率
    uint count;        // 今回生成する粒数
    uint emit;         // 発生要求がある場合は1
    float padding;     // 定数バッファ境界用の余白
};

struct PerFrame
{
    float time;        // 初期化後の累積秒数
    float deltaTime;   // 今フレームの経過秒数
    float2 padding;    // 定数バッファ境界用の余白
};

float rand3dTo1d(float3 value)
{
    return frac(sin(dot(value, float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
}

float3 rand3dTo3d(float3 value)
{
    return float3(
        rand3dTo1d(value),
        rand3dTo1d(value + float3(31.416f, 47.853f, 12.793f)),
        rand3dTo1d(value + float3(74.723f, 15.682f, 55.234f))
    );
}

class RandomGenerator
{
    float3 seed;

    float3 Generate3d()
    {
        seed = rand3dTo3d(seed);
        return seed;
    }

    float Generate1d()
    {
        float result = rand3dTo1d(seed);
        seed.x = result;
        return result;
    }
};

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1); // FreeList末尾の有効番号
RWStructuredBuffer<uint> gFreeList : register(u2);     // 未使用粒子のスロット番号
ConstantBuffer<EmitterSphere> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0)
    {
        return;
    }

    RandomGenerator generator;
    generator.seed =
        float3(dispatchThreadId.x + 1.0f, gPerFrame.time, gPerFrame.deltaTime + 1.0f) *
        (gPerFrame.time + 1.0f);

    for (uint countIndex = 0; countIndex < gEmitter.count; ++countIndex)
    {
        int freeListIndex;
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

        if (0 <= freeListIndex && freeListIndex < kMaxParticles)
        {
            uint particleIndex = gFreeList[freeListIndex];
            float3 randomPosition = generator.Generate3d() * 2.0f - 1.0f;
            float2 randomScale = lerp(gEmitter.scaleMin, gEmitter.scaleMax, generator.Generate3d().xy);
            float3 randomDirection = generator.Generate3d() * 2.0f - 1.0f;
            float3 emissionDirection = normalize(gEmitter.direction + randomDirection * gEmitter.spread);
            float speed = lerp(gEmitter.speedMin, gEmitter.speedMax, generator.Generate1d());
            float colorRate = generator.Generate1d();

            gParticles[particleIndex] = (Particle)0;
            gParticles[particleIndex].translate =
                gEmitter.translate + randomPosition * gEmitter.radius;
            gParticles[particleIndex].scale = float3(randomScale, 1.0f);
            gParticles[particleIndex].initialScale = float3(randomScale, 1.0f);
            gParticles[particleIndex].endScale = gEmitter.endScale;
            gParticles[particleIndex].lifeTime =
                lerp(gEmitter.lifeTimeMin, gEmitter.lifeTimeMax, generator.Generate1d());
            gParticles[particleIndex].velocity = emissionDirection * speed;
            gParticles[particleIndex].currentTime = 0.0f;
            gParticles[particleIndex].color = lerp(gEmitter.colorMin, gEmitter.colorMax, colorRate);
        }
        else
        {
            // InterlockedAddで先に減らした分を戻し、FreeListの有効範囲を維持する。
            int unused;
            InterlockedAdd(gFreeListIndex[0], 1, unused);
            break;
        }
    }
}
