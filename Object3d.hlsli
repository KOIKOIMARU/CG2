struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : TEXCOORD1;
};

struct Material
{
    float4 color;
    int enableLighting;
    float shininess;
    float3 specularColor;
    float pad0;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

struct Camera
{
    float3 worldPosition;
    float padding;
};

struct PointLight
{
    float4 color; // ライトの色
    float3 position; // ライトの位置
    float intensity; // 輝度
    float radius; // 届く最大距離
    float decay; // 減衰率（大きいほど急減衰）
};

struct SpotLight
{
    float4 color; // ライトの色
    float3 position; // ライトの位置
    float intensity; // 輝度

    float3 direction; // スポットライトの方向（正規化前提）
    float distance; // ライトの届く最大距離

    float decay; // 減衰率
    float cosAngle; // 終端角（ここで0になる）cos
    float cosFalloffStart; // 開始角（ここまでは1）cos
    float padding; // 16byte境界合わせ
};
