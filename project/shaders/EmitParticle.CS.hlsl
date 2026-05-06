static const uint kMaxParticles = 1024;

struct Particle
{
    float3 translate;
    float3 scale;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
};

struct EmitterSphere
{
    float3 translate;
    float radius;
    uint count;
    float frequency;
    float frequencyTime;
    uint emit;
};

struct PerFrame
{
    float time;
    float deltaTime;
    float2 padding;
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
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);
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
            float randomScale = 0.12f + generator.Generate1d() * 0.18f;
            float3 randomVelocity = normalize(randomPosition + float3(0.0f, 0.8f, 0.0f));

            gParticles[particleIndex] = (Particle)0;
            gParticles[particleIndex].translate =
                gEmitter.translate + randomPosition * gEmitter.radius;
            gParticles[particleIndex].scale = float3(randomScale, randomScale, randomScale);
            gParticles[particleIndex].lifeTime = 1.0f + generator.Generate1d();
            gParticles[particleIndex].velocity =
                randomVelocity * (0.4f + generator.Generate1d() * 1.2f);
            gParticles[particleIndex].currentTime = 0.0f;
            gParticles[particleIndex].color =
                float4(generator.Generate3d(), 1.0f);
        }
        else
        {
            // 空きがないのに減らしてしまった分を戻して、このEmitを終える
            int unused;
            InterlockedAdd(gFreeListIndex[0], 1, unused);
            break;
        }
    }
}
