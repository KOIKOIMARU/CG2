#include "Object3D.hlsli"

cbuffer MaterialCB : register(b0)
{
    Material gMaterial;
};
cbuffer CameraCB : register(b2)
{
    Camera gCamera;
};
cbuffer LightCB : register(b3)
{
    DirectionalLight gDirectionalLight;
};

// ★追加：PointLight (b4)
cbuffer PointLightCB : register(b4)
{
    PointLight gPointLight;
};

// ★追加：SpotLight (b5)
cbuffer SpotLightCB : register(b5)
{
    SpotLight gSpotLight;
};


Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    if (gMaterial.enableLighting != 0)
    {
        float3 N = normalize(input.normal);
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        // =========================
        // Directional Light
        // =========================
        float3 Ld = normalize(-gDirectionalLight.direction.xyz);

        float NdotLd = dot(N, Ld);
        float halfLambert = pow(NdotLd * 0.5f + 0.5f, 2.0f);

        float3 diffuseDir =
            gMaterial.color.rgb * textureColor.rgb *
            gDirectionalLight.color.rgb * halfLambert *
            gDirectionalLight.intensity;

        float3 Hd = normalize(Ld + toEye);
        float NdotHd = saturate(dot(N, Hd));
        float specularPowDir = pow(NdotHd, gMaterial.shininess);

        float3 specularDir =
            gDirectionalLight.color.rgb *
            gDirectionalLight.intensity *
            specularPowDir *
            gMaterial.specularColor; // ★specularColor を反映（君のMaterialにあるので使う）

        // =========================
        // Point Light (with attenuation)
        // =========================
        float3 LpVec = gPointLight.position - input.worldPosition;
        float dist = length(LpVec);

        // 距離0で最大、radius以上で0、decayで曲線を調整
        float factor = pow(saturate(1.0f - dist / gPointLight.radius), gPointLight.decay);

        float3 Lp = (dist > 0.0001f) ? (LpVec / dist) : float3(0.0f, 1.0f, 0.0f);

        float NdotLp = dot(N, Lp);
        float halfLambertP = pow(NdotLp * 0.5f + 0.5f, 2.0f);

        float3 diffusePt =
            gMaterial.color.rgb * textureColor.rgb *
            gPointLight.color.rgb * halfLambertP *
            (gPointLight.intensity * factor);

        float3 Hp = normalize(Lp + toEye);
        float NdotHp = saturate(dot(N, Hp));
        float specularPowPt = pow(NdotHp, gMaterial.shininess);

        float3 specularPt =
            gPointLight.color.rgb *
            (gPointLight.intensity * factor) *
            specularPowPt *
            gMaterial.specularColor;
        
                // =========================
        // Spot Light (distance attenuation + falloff)
        // =========================

        // ライト -> 表面方向（入射光）
        float3 LsVec = input.worldPosition - gSpotLight.position;
        float distS = length(LsVec);
        float3 Ls = (distS > 0.0001f) ? (LsVec / distS) : float3(0.0f, 1.0f, 0.0f);

        // 距離減衰（Pointと同じ形）
        float attenS = pow(saturate(1.0f - distS / gSpotLight.distance), gSpotLight.decay);

        // 角度Falloff
        float3 spotDir = normalize(gSpotLight.direction);
        float cosTheta = dot(Ls, spotDir);

        // cosFalloffStart と cosAngle が同じだと0除算になるので、C++側で同値禁止にする前提
        float denom = (gSpotLight.cosFalloffStart - gSpotLight.cosAngle);
        float falloff = saturate((cosTheta - gSpotLight.cosAngle) / denom);

        float spotFactor = gSpotLight.intensity * attenS * falloff;

        // diffuse
        float NdotLs = dot(N, Ls);
        float halfLambertS = pow(NdotLs * 0.5f + 0.5f, 2.0f);

        float3 diffuseSp =
            gMaterial.color.rgb * textureColor.rgb *
            gSpotLight.color.rgb * halfLambertS *
            spotFactor;

        // specular
        float3 Hs = normalize(Ls + toEye);
        float NdotHs = saturate(dot(N, Hs));
        float specularPowSp = pow(NdotHs, gMaterial.shininess);

        float3 specularSp =
            gSpotLight.color.rgb *
            spotFactor *
            specularPowSp *
            gMaterial.specularColor;


        // =========================
        // Sum
        // =========================
        output.color.rgb = diffuseDir + specularDir + diffusePt + specularPt + diffuseSp + specularSp;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    return output;
}
