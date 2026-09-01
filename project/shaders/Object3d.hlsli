// 通常3D描画の頂点・マテリアル・ライト定義。C++側の同名構造体と並びを合わせる。
struct VertexShaderOutput
{
    float4 position : SV_POSITION;    // 射影後のクリップ座標
    float2 texcoord : TEXCOORD0;      // マテリアルテクスチャのUV座標
    float3 normal : NORMAL0;          // ワールド空間の頂点法線
    float3 worldPosition : TEXCOORD1; // ライティングに使うワールド位置
};

struct Material
{
    float4 color;                    // ベースカラーへ乗算するRGBA
    int lightingMode;                // 使用するライティング方式番号
    float shininess;                 // 鏡面反射の鋭さ
    float environmentCoefficient;    // 環境マップ反射の合成率
    float alphaReference;            // アルファカットアウトのしきい値
    float3 specularColor;            // 鏡面反射色
    float roughness;                 // PBRの表面粗さ
    float metallic;                  // PBRの金属度
    float normalStrength;            // 法線マップの凹凸強度
    float2 padding1;                 // 定数バッファ境界用の余白
    float4x4 uvTransform;            // UVの拡縮・回転・移動
};

struct DirectionalLight
{
    float4 color;                    // 平行光源のRGBA色
    float3 direction;                // 光が進むワールド空間方向
    float intensity;                 // 光量
    float4x4 lightViewProjection;    // 影マップへ投影する変換
    float shadowStrength;            // 影を暗くする割合
    float shadowBias;                // 深度比較時の固定補正
    float shadowNormalBias;          // 面方向に応じた深度補正
    float shadowMapEnabled;          // 影マップが有効なら1
};

struct Camera
{
    float3 worldPosition; // 鏡面反射の視線計算に使うカメラ位置
    float padding;        // 定数バッファ境界用の余白
};

struct WellForGPU
{
    float4x4 skeletonSpaceMatrix;                 // 頂点位置を現在姿勢へ変換する行列
    float4x4 skeletonSpaceInverseTransposeMatrix; // 頂点法線を現在姿勢へ変換する行列
};

struct PointLight
{
    float4 color;     // 点光源のRGBA色
    float3 position;  // ワールド空間の発光位置
    float intensity;  // 光量
    float radius;     // 光が届く最大距離
    float decay;      // 距離減衰の指数
    float2 padding;   // 定数バッファ境界用の余白
};

struct SpotLight
{
    float4 color;         // スポットライトのRGBA色
    float3 position;      // ワールド空間の発光位置
    float intensity;      // 光量
    float3 direction;     // 光が進むワールド空間方向
    float distance;       // 光が届く最大距離
    float decay;          // 距離減衰の指数
    float cosAngle;       // 照射範囲外端の角度余弦
    float cosFalloffStart;// 減衰開始位置の角度余弦
    float padding;        // 定数バッファ境界用の余白
};
