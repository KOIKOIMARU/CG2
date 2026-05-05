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

RWStructuredBuffer<Particle> gParticles : register(u0);

[numthreads(1024, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    gParticles[particleIndex] = (Particle)0;
    gParticles[particleIndex].scale = float3(0.5f, 0.5f, 0.5f);
    gParticles[particleIndex].color = float4(1.0f, 1.0f, 1.0f, 1.0f);
}
