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
#define MAX_POINT_LIGHTS 4

cbuffer PointLightCB : register(b4)
{
    PointLight gPointLights[MAX_POINT_LIGHTS];
    int gPointCount;
    float3 _padPoint; // 16byte境界
};

#define MAX_SPOT_LIGHTS 4

cbuffer SpotLightCB : register(b5)
{
    SpotLight gSpotLights[MAX_SPOT_LIGHTS];
    int gSpotCount;
    float3 _padSpot; // 16byte境界
};

struct RectLight
{
    float4 color; // rgb: 色, a未使用
    float3 position; // 矩形中心（World）
    float intensity;

    float3 right; // 矩形の横方向（正規化推奨）
    float halfWidth; // 横の半分

    float3 up; // 矩形の縦方向（正規化推奨）
    float halfHeight; // 縦の半分

    float radius; // 影響範囲（距離減衰用）
    float decay; // 減衰
    float2 padding; // 16byte境界
};

#define MAX_RECT_LIGHTS 2

cbuffer RectLightCB : register(b6)
{
    RectLight gRectLights[MAX_RECT_LIGHTS];
    int gRectCount;
    float3 _padRect;
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
// Point Lights (multiple)
// =========================
        float3 diffusePtSum = 0.0f;
        float3 specularPtSum = 0.0f;

        int pc = min(gPointCount, MAX_POINT_LIGHTS);
        for (int i = 0; i < pc; ++i)
        {
            PointLight pl = gPointLights[i];

            float3 LpVec = pl.position - input.worldPosition;
            float dist = length(LpVec);

            float factor = pow(saturate(1.0f - dist / pl.radius), pl.decay);
            float3 Lp = (dist > 1e-4f) ? (LpVec / dist) : float3(0, 1, 0);

            float NdotLp = dot(N, Lp);
            float halfLambertP = pow(NdotLp * 0.5f + 0.5f, 2.0f);

            diffusePtSum +=
        gMaterial.color.rgb * textureColor.rgb *
        pl.color.rgb * halfLambertP *
        (pl.intensity * factor);

            float3 Hp = normalize(Lp + toEye);
            float specPow = pow(saturate(dot(N, Hp)), gMaterial.shininess);

            specularPtSum +=
        pl.color.rgb *
        (pl.intensity * factor) *
        specPow *
        gMaterial.specularColor;
        }

      // =========================
// Spot Lights (multiple)
// =========================
        float3 diffuseSpSum = 0.0f;
        float3 specularSpSum = 0.0f;

        int sc = min(gSpotCount, MAX_SPOT_LIGHTS);
        for (int i = 0; i < sc; ++i)
        {
            SpotLight sl = gSpotLights[i];

    // surface -> light
            float3 toLight = sl.position - input.worldPosition;
            float distS = length(toLight);
            float3 L = (distS > 1e-4f) ? (toLight / distS) : float3(0, 1, 0);

    // distance attenuation
            float attenS = pow(saturate(1.0f - distS / sl.distance), sl.decay);

    // angle falloff (スポット中心方向は sl.direction)
            float3 lightToSurf = (distS > 1e-4f) ? (-L) : float3(0, -1, 0); // light -> surface
            float3 spotDir = normalize(sl.direction);
            float cosTheta = dot(lightToSurf, spotDir);

            float denom = max(sl.cosFalloffStart - sl.cosAngle, 1e-4f);
            float falloff = saturate((cosTheta - sl.cosAngle) / denom);

            float spotFactor = sl.intensity * attenS * falloff;

    // diffuse
            float NdotL = dot(N, L);
            float halfLambertS = pow(NdotL * 0.5f + 0.5f, 2.0f);

            diffuseSpSum +=
        gMaterial.color.rgb * textureColor.rgb *
        sl.color.rgb * halfLambertS *
        spotFactor;

    // specular (Blinn-Phong)
            float3 H = normalize(L + toEye);
            float specPow = pow(saturate(dot(N, H)), gMaterial.shininess);

            specularSpSum +=
        sl.color.rgb *
        spotFactor *
        specPow *
        gMaterial.specularColor;
        }

        // =========================
// Rect Lights (multiple) : pseudo area light (sampled points)
// =========================
        float3 diffuseRectSum = 0.0f;
        float3 specularRectSum = 0.0f;

#define RECT_SAMPLES_1D 3
#define RECT_SAMPLES (RECT_SAMPLES_1D*RECT_SAMPLES_1D)

        int rc = min(gRectCount, MAX_RECT_LIGHTS);
        for (int r = 0; r < rc; ++r)
        {
            RectLight rl = gRectLights[r];

            float3 right = normalize(rl.right);
            float3 up = normalize(rl.up);

    // 3x3 サンプル
    [unroll]
            for (int y = 0; y < RECT_SAMPLES_1D; ++y)
            {
                float fy = ((y + 0.5f) / RECT_SAMPLES_1D) * 2.0f - 1.0f; // -1..1
        [unroll]
                for (int x = 0; x < RECT_SAMPLES_1D; ++x)
                {
                    float fx = ((x + 0.5f) / RECT_SAMPLES_1D) * 2.0f - 1.0f;

                    float3 samplePos =
                rl.position +
                right * (fx * rl.halfWidth) +
                up * (fy * rl.halfHeight);

                    float3 toLight = samplePos - input.worldPosition;
                    float dist = length(toLight);
                    float3 L = (dist > 1e-4f) ? (toLight / dist) : float3(0, 1, 0);

            // 距離減衰（Pointと同じ形）
                    float atten = pow(saturate(1.0f - dist / rl.radius), rl.decay);

            // 9点に分割するので割る
                    float eachI = (rl.intensity * atten) / RECT_SAMPLES;

            // diffuse（Half-Lambert）
                    float NdotLr = dot(N, L);
                    float halfLambertR = pow(NdotLr * 0.5f + 0.5f, 2.0f);

                    diffuseRectSum +=
                gMaterial.color.rgb * textureColor.rgb *
                rl.color.rgb * halfLambertR *
                eachI;

            // specular（Blinn-Phong）
                    float3 H = normalize(L + toEye);
                    float specPow = pow(saturate(dot(N, H)), gMaterial.shininess);

                    specularRectSum +=
                rl.color.rgb *
                eachI *
                specPow *
                gMaterial.specularColor;
                }
            }
        }


        // =========================
        // Sum
        // =========================
        output.color.rgb =
    diffuseDir + specularDir +
    diffusePtSum + specularPtSum +
    diffuseSpSum + specularSpSum +
    diffuseRectSum + specularRectSum;

        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    return output;
}
