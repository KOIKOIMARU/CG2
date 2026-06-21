#include "Object3d.hlsli" 

cbuffer MaterialCB : register(b0)
{
    Material gMaterial;
};

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
SamplerState gSampler : register(s0);

cbuffer CameraCB : register(b2)
{
    Camera gCamera;
};

cbuffer DirectionalLightCB : register(b3)
{
    DirectionalLight gDirectionalLight;
};

cbuffer PointLightCB : register(b4)
{
    PointLight gPointLight;
};

cbuffer SpotLightCB : register(b5)
{
    SpotLight gSpotLight;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 tex = gTexture.Sample(gSampler, uv);
    float3 normal = normalize(input.normal);

    if (tex.a <= gMaterial.alphaReference)
    {
        discard;
    }

    if (gMaterial.lightingMode != 0)
    {
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        float3 Ld = normalize(-gDirectionalLight.direction);
        float NdotLd = dot(normal, Ld);
        float diffuseFactorDir = saturate(NdotLd);
        if (gMaterial.lightingMode == 2) {
            float halfLambert = NdotLd * 0.5f + 0.5f;
            diffuseFactorDir = halfLambert * halfLambert;
        }

        float3 diffuseDir =
            gMaterial.color.rgb * tex.rgb *
            gDirectionalLight.color.rgb *
            diffuseFactorDir *
            gDirectionalLight.intensity;

        float3 Hd = normalize(Ld + toEye);
        float specularPowDir =
            pow(saturate(dot(normal, Hd)), gMaterial.shininess);
        float3 specularDir =
            gDirectionalLight.color.rgb *
            gDirectionalLight.intensity *
            specularPowDir *
            gMaterial.specularColor;

        float3 LpVec = gPointLight.position - input.worldPosition;
        float distP = length(LpVec);
        float3 Lp =
            (distP > 0.0001f) ? (LpVec / distP) : float3(0.0f, 1.0f, 0.0f);
        float attenP =
            pow(saturate(1.0f - distP / gPointLight.radius), gPointLight.decay);
        float NdotLp = dot(normal, Lp);
        float diffuseFactorPoint = saturate(NdotLp);
        if (gMaterial.lightingMode == 2) {
            float halfLambertP = NdotLp * 0.5f + 0.5f;
            diffuseFactorPoint = halfLambertP * halfLambertP;
        }

        float3 diffusePoint =
            gMaterial.color.rgb * tex.rgb *
            gPointLight.color.rgb *
            diffuseFactorPoint *
            gPointLight.intensity *
            attenP;

        float3 Hp = normalize(Lp + toEye);
        float specularPowPoint =
            pow(saturate(dot(normal, Hp)), gMaterial.shininess);
        float3 specularPoint =
            gPointLight.color.rgb *
            gPointLight.intensity *
            attenP *
            specularPowPoint *
            gMaterial.specularColor;

        float3 LsFromLight = input.worldPosition - gSpotLight.position;
        float distS = length(LsFromLight);
        float3 Ls =
            (distS > 0.0001f) ?
            (LsFromLight / distS) :
            float3(0.0f, -1.0f, 0.0f);

        float attenS =
            pow(saturate(1.0f - distS / gSpotLight.distance), gSpotLight.decay);
        float cosTheta = dot(Ls, normalize(gSpotLight.direction));
        float denom =
            max(gSpotLight.cosFalloffStart - gSpotLight.cosAngle, 0.0001f);
        float falloff =
            saturate((cosTheta - gSpotLight.cosAngle) / denom);
        float spotFactor = gSpotLight.intensity * attenS * falloff;

        float3 lightToSurface = -Ls;
        float NdotLs = dot(normal, lightToSurface);
        float diffuseFactorSpot = saturate(NdotLs);
        if (gMaterial.lightingMode == 2) {
            float halfLambertS = NdotLs * 0.5f + 0.5f;
            diffuseFactorSpot = halfLambertS * halfLambertS;
        }

        float3 diffuseSpot =
            gMaterial.color.rgb * tex.rgb *
            gSpotLight.color.rgb *
            diffuseFactorSpot *
            spotFactor;

        float3 Hs = normalize(lightToSurface + toEye);
        float specularPowSpot =
            pow(saturate(dot(normal, Hs)), gMaterial.shininess);
        float3 specularSpot =
            gSpotLight.color.rgb *
            spotFactor *
            specularPowSpot *
            gMaterial.specularColor;

        float3 cameraToPosition =
            normalize(input.worldPosition - gCamera.worldPosition);
        float3 reflectedVector = reflect(cameraToPosition, normal);
        float3 environmentColor =
            gEnvironmentTexture.Sample(gSampler, reflectedVector).rgb *
            gMaterial.environmentCoefficient;

        output.color.rgb =
            diffuseDir + specularDir +
            diffusePoint + specularPoint +
            diffuseSpot + specularSpot +
            environmentColor;
        output.color.a = gMaterial.color.a * tex.a;
    }
    else
    {
        output.color = gMaterial.color * tex;
    }
    
    return output;

}

