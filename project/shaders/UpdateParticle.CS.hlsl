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

struct PerFrame
{
    float time;
    float deltaTime;
    float2 padding;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
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
    }
}
