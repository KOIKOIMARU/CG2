#include "Object3d.hlsli" 

// テクスチャ、直接光、環境光、法線マップ、影を統合する通常3D描画用シェーダー。
cbuffer MaterialCB : register(b0)
{
    Material gMaterial;
};

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
Texture2D<float> gShadowMap : register(t2);
Texture2D<float4> gNormalTexture : register(t3);
SamplerState gSampler : register(s0);
SamplerState gShadowSampler : register(s1);

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

float CalculateDirectionalShadow(float3 worldPosition, float3 normal, float3 lightToSurface)
{
    // 4点PCFで影境界を平滑化し、biasで自己シャドウのちらつきを抑える。
    if (gDirectionalLight.shadowMapEnabled < 0.5f ||
        gDirectionalLight.shadowStrength <= 0.001f)
    {
        return 1.0f;
    }

    float4 lightClip =
        mul(float4(worldPosition, 1.0f), gDirectionalLight.lightViewProjection);
    if (lightClip.w <= 0.0001f)
    {
        return 1.0f;
    }

    float3 lightNdc = lightClip.xyz / lightClip.w;
    if (lightNdc.z < 0.0f || lightNdc.z > 1.0f)
    {
        return 1.0f;
    }

    float2 shadowUv = lightNdc.xy * float2(0.5f, -0.5f) + 0.5f;
    if (any(shadowUv < 0.002f) || any(shadowUv > 0.998f))
    {
        return 1.0f;
    }

    uint shadowWidth = 0;
    uint shadowHeight = 0;
    gShadowMap.GetDimensions(shadowWidth, shadowHeight);
    float2 texelSize = 1.0f / float2(shadowWidth, shadowHeight);

    float normalBias =
        (1.0f - saturate(dot(normal, lightToSurface))) *
        gDirectionalLight.shadowNormalBias;
    float receiverDepth = lightNdc.z - gDirectionalLight.shadowBias - normalBias;

    float visibility = 0.0f;
    float2 sampleOffset = texelSize * 0.75f;
    float sampledDepth =
        gShadowMap.Sample(gShadowSampler, shadowUv + float2(-sampleOffset.x, -sampleOffset.y));
    visibility += receiverDepth <= sampledDepth ? 1.0f : 0.0f;
    sampledDepth =
        gShadowMap.Sample(gShadowSampler, shadowUv + float2(sampleOffset.x, -sampleOffset.y));
    visibility += receiverDepth <= sampledDepth ? 1.0f : 0.0f;
    sampledDepth =
        gShadowMap.Sample(gShadowSampler, shadowUv + float2(-sampleOffset.x, sampleOffset.y));
    visibility += receiverDepth <= sampledDepth ? 1.0f : 0.0f;
    sampledDepth =
        gShadowMap.Sample(gShadowSampler, shadowUv + float2(sampleOffset.x, sampleOffset.y));
    visibility += receiverDepth <= sampledDepth ? 1.0f : 0.0f;
    visibility *= 0.25f;

    return lerp(1.0f - gDirectionalLight.shadowStrength, 1.0f, visibility);
}

float3 SampleEnvironment(float3 direction)
{
    return gEnvironmentTexture.Sample(gSampler, normalize(direction)).rgb;
}

float3 CalculateDiffuseEnvironment(float3 normal, float roughness, float metallic)
{
    float upFactor = saturate(normal.y * 0.5f + 0.5f);
    float3 skyDirection = normalize(normal + float3(0.0f, 0.55f, 0.0f));
    float3 horizonDirection =
        normalize(float3(normal.x, 0.18f, normal.z) + float3(0.0f, 0.0f, 0.001f));

    float3 skyColor = SampleEnvironment(skyDirection);
    float3 horizonColor = SampleEnvironment(horizonDirection);
    float3 groundBounce = float3(0.40f, 0.43f, 0.48f);
    float3 upperHemisphere = lerp(horizonColor, skyColor, upFactor);
    float3 hemisphereColor = lerp(groundBounce, upperHemisphere, upFactor);

    float surfaceWrap = lerp(0.78f, 1.04f, upFactor);
    float diffuseStrength =
        lerp(0.135f, 0.205f, roughness) *
        lerp(1.0f, 0.52f, metallic);
    return hemisphereColor * surfaceWrap * diffuseStrength;
}

float3 ApplyNormalMap(
    float3 geometryNormal,
    float3 worldPosition,
    float2 uv,
    float normalStrength)
{
    float strength = saturate(normalStrength);
    if (strength <= 0.001f)
    {
        return geometryNormal;
    }

    float3 dp1 = ddx(worldPosition);
    float3 dp2 = ddy(worldPosition);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 dp2Perpendicular = cross(dp2, geometryNormal);
    float3 dp1Perpendicular = cross(geometryNormal, dp1);
    float3 tangent = dp2Perpendicular * duv1.x + dp1Perpendicular * duv2.x;
    float3 bitangent = dp2Perpendicular * duv1.y + dp1Perpendicular * duv2.y;

    float tangentLength = dot(tangent, tangent);
    float bitangentLength = dot(bitangent, bitangent);
    float inverseLength = rsqrt(max(max(tangentLength, bitangentLength), 0.000001f));
    tangent *= inverseLength;
    bitangent *= inverseLength;

    float3 tangentNormal = gNormalTexture.Sample(gSampler, uv).xyz * 2.0f - 1.0f;
    tangentNormal.xy *= strength;
    tangentNormal = normalize(tangentNormal);

    return normalize(
        tangent * tangentNormal.x +
        bitangent * tangentNormal.y +
        geometryNormal * tangentNormal.z);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 tex = gTexture.Sample(gSampler, uv);
    float3 geometryNormal = normalize(input.normal);
    float3 normal = ApplyNormalMap(
        geometryNormal,
        input.worldPosition,
        uv,
        gMaterial.normalStrength);
    float3 baseColor = gMaterial.color.rgb * tex.rgb;
    float roughness = max(saturate(gMaterial.roughness), 0.04f);
    float metallic = saturate(gMaterial.metallic);
    float3 diffuseAlbedo = baseColor * (1.0f - metallic * 0.65f);
    float3 specularTint = lerp(gMaterial.specularColor, baseColor, metallic);
    float specularPower = lerp(max(gMaterial.shininess, 1.0f), 12.0f, roughness);
    float specularEnergy =
        lerp(0.95f, 0.24f, roughness) *
        lerp(1.0f, 1.45f, metallic);

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

        float directionalShadow =
            CalculateDirectionalShadow(input.worldPosition, normal, Ld);

        float3 diffuseDir =
            diffuseAlbedo *
            gDirectionalLight.color.rgb *
            diffuseFactorDir *
            gDirectionalLight.intensity *
            directionalShadow;

        float3 Hd = normalize(Ld + toEye);
        float specularPowDir =
            pow(saturate(dot(normal, Hd)), specularPower);
        float3 specularDir =
            gDirectionalLight.color.rgb *
            gDirectionalLight.intensity *
            specularPowDir *
            specularTint *
            specularEnergy *
            directionalShadow;

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
            diffuseAlbedo *
            gPointLight.color.rgb *
            diffuseFactorPoint *
            gPointLight.intensity *
            attenP;

        float3 Hp = normalize(Lp + toEye);
        float specularPowPoint =
            pow(saturate(dot(normal, Hp)), specularPower);
        float3 specularPoint =
            gPointLight.color.rgb *
            gPointLight.intensity *
            attenP *
            specularPowPoint *
            specularTint *
            specularEnergy;

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
            diffuseAlbedo *
            gSpotLight.color.rgb *
            diffuseFactorSpot *
            spotFactor;

        float3 Hs = normalize(lightToSurface + toEye);
        float specularPowSpot =
            pow(saturate(dot(normal, Hs)), specularPower);
        float3 specularSpot =
            gSpotLight.color.rgb *
            spotFactor *
            specularPowSpot *
            specularTint *
            specularEnergy;

        float3 cameraToPosition =
            normalize(input.worldPosition - gCamera.worldPosition);
        float3 reflectedVector = reflect(cameraToPosition, normal);
        float viewFresnel =
            pow(1.0f - saturate(dot(normal, toEye)), 4.0f);
        float environmentScale =
            lerp(1.10f, 0.38f, roughness) *
            lerp(1.0f, 1.55f, metallic) *
            lerp(0.82f, 1.28f, viewFresnel);
        float3 environmentColor =
            lerp(
                SampleEnvironment(reflectedVector),
                SampleEnvironment(normal + float3(0.0f, 0.35f, 0.0f)),
                saturate(roughness * 0.70f)) *
            gMaterial.environmentCoefficient *
            environmentScale;
        float3 diffuseEnvironment =
            diffuseAlbedo *
            CalculateDiffuseEnvironment(normal, roughness, metallic);
        float rimFactor = pow(
            1.0f - saturate(dot(normal, toEye)),
            lerp(3.4f, 2.5f, metallic));
        float rimVisibility =
            saturate(0.32f + (1.0f - diffuseFactorDir) * 0.68f);
        float3 stylizedRim =
            lerp(float3(0.34f, 0.62f, 1.0f), specularTint, metallic * 0.42f) *
            rimFactor *
            rimVisibility *
            lerp(0.024f, 0.052f, 1.0f - roughness);

        output.color.rgb =
            diffuseDir + specularDir +
            diffusePoint + specularPoint +
            diffuseSpot + specularSpot +
            diffuseEnvironment +
            environmentColor +
            stylizedRim;
        output.color.a = gMaterial.color.a * tex.a;
    }
    else
    {
        output.color = gMaterial.color * tex;
    }
    
    return output;

}

