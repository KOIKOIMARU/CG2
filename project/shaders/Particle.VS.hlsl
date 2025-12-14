struct VertexIn
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct VertexOut
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

struct InstanceData
{
    float4x4 WVP;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

// t1: StructuredBuffer（ParticleInstanceData配列）
StructuredBuffer<InstanceData> gInstances : register(t1);

VertexOut main(VertexIn vin, uint instanceId : SV_InstanceID)
{
    VertexOut vout;
    float4x4 wvp = gInstances[instanceId].WVP;

    vout.position = mul(wvp, vin.position);
    vout.texcoord = vin.texcoord;
    return vout;
}
