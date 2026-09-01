#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"

using Microsoft::WRL::ComPtr;
using namespace Math;

constexpr uint32_t kNumMaxInfluence = 4;

// CPU側で保持する頂点、マテリアル、アニメーション、スケルトンの読み込み結果。
// VertexDataの配置は頂点シェーダーおよびCompute Skinningの入力定義と一致させる。
struct VertexData {
    Vector4 position;                                      // モデル空間の頂点位置
    Vector2 texcoord;                                      // テクスチャ参照用のUV座標
    Vector3 normal;                                        // ライティングに使うモデル空間法線
    Vector4 weight;                                        // 各ジョイントが頂点へ与える影響率
    std::array<uint32_t, kNumMaxInfluence> jointIndices;   // weightの各要素に対応するジョイント番号
};

struct MaterialData {
    std::string textureFilePath;        // ベースカラーのテクスチャ
    std::string normalTextureFilePath;  // 法線マップ。空なら使用しない
    float roughness = 0.66f;            // 表面の粗さ。大きいほど反射がぼける
    float metallic = 0.0f;              // 金属として扱う割合
    float normalStrength = 0.0f;        // 法線マップの凹凸強度
};

struct ModelDrawRange {
    uint32_t indexOffset = 0;    // インデックスバッファ内の描画開始位置
    uint32_t indexCount = 0;     // この範囲で描画するインデックス数
    uint32_t materialIndex = 0;  // 使用するmaterialsの番号
};

template <typename T>
struct Keyframe {
    float time; // アニメーション開始からの時刻
    T value;    // その時刻における姿勢値
};

struct NodeAnimation {
    std::vector<Keyframe<Vector3>> translate;   // 位置のキーフレーム列
    std::vector<Keyframe<Quaternion>> rotate;   // 回転のキーフレーム列
    std::vector<Keyframe<Vector3>> scale;       // 拡縮のキーフレーム列
};

struct Animation {
    float duration = 0.0f;                               // アニメーション全体の長さ（tick）
    float ticksPerSecond = 1.0f;                         // 1秒あたりに進めるtick数
    std::map<std::string, NodeAnimation> nodeAnimations; // ノード名ごとの姿勢変化
};

struct Node {
    QuaternionTransform transform; // 親ノードから見た初期姿勢
    Matrix4x4 localMatrix;          // transformを行列化したローカル変換
    std::string name;               // アニメーションやボーンとの照合名
    std::vector<Node> children;     // このノードを親に持つ子ノード
};

struct JointWeightData {
    Matrix4x4 inverseBindPoseMatrix; // 初期姿勢を打ち消してボーン変形へ移す行列
};

struct SkinClusterData {
    std::map<std::string, JointWeightData> jointWeights; // ジョイント名ごとのスキニング情報
};

struct Joint {
    QuaternionTransform transform;         // 現在のローカル姿勢
    QuaternionTransform bindPoseTransform; // モデル読込時の基準姿勢
    Matrix4x4 localMatrix;                  // 現在姿勢のローカル行列
    Matrix4x4 skeletonSpaceMatrix;          // ルートから累積したスケルトン空間行列
    std::string name;                       // アニメーションとの照合名
    std::vector<int32_t> children;          // 子ジョイントの番号
    std::optional<int32_t> parent;          // 親ジョイント番号。ルートなら値なし
    int32_t index;                          // Skeleton::joints内での自分の番号
};

struct Skeleton {
    int32_t root = -1;                       // ルートジョイント番号
    std::map<std::string, int32_t> jointMap; // 名前からジョイント番号を引く表
    std::vector<Joint> joints;               // 階層を平坦化した全ジョイント
};

struct ModelData {
    std::vector<VertexData> vertices;       // CPU側の全頂点
    std::vector<uint32_t> indices;          // CPU側の三角形インデックス
    MaterialData material;                  // 単一マテリアル互換用の代表値
    std::vector<MaterialData> materials;    // メッシュが参照する全マテリアル
    std::vector<ModelDrawRange> drawRanges; // マテリアル単位の描画範囲
    Node rootNode;                          // 読み込んだノード階層のルート
    Animation animation;                    // モデルに含まれるアニメーション
    SkinClusterData skinClusterData;        // ボーン変形に必要な逆バインド情報
};

// HLSLのマテリアル定数バッファと同じ並びを保つGPU転送構造体。
struct Material {
    Vector4 color;                 // ベースカラーへ乗算するRGBA
    int32_t lightingMode;          // 使用するライティング計算方式
    float shininess;               // 鏡面反射の鋭さ
    float environmentCoefficient;  // 環境マップ反射の合成率
    float alphaReference;          // アルファカットアウトのしきい値
    Vector3 specularColor;         // 鏡面反射色
    float roughness;               // PBRの表面粗さ
    float metallic;                // PBRの金属度
    float normalStrength;          // 法線マップの強度
    float padding1[2];             // 16バイト境界にそろえる余白
    Matrix4x4 uvTransform;         // UVの拡縮・回転・移動
};

class ModelCommon;

// 1つのモデル資源を所有し、頂点・インデックス・マテリアルを描画する。
// ModelManagerが生成と寿命を管理し、Object3dはこのクラスを借用する。
class Model {
public:
    // ファイルまたは生成済みデータからGPU資源を構築する。どちらか一方を一度だけ呼ぶ。
    void Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);
    void Initialize(ModelCommon* modelCommon, const ModelData& modelData);
    void Draw(
        const D3D12_VERTEX_BUFFER_VIEW* overrideVertexBufferView = nullptr,
        ID3D12Resource* overrideMaterialResource = nullptr,
        const std::string* overrideTextureFilePath = nullptr);
    void SetEnvironmentCoefficient(float coefficient);
    float GetEnvironmentCoefficient() const;
    void SetColor(const Vector4& color);
    void SetAlphaReference(float alphaReference);
    void SetUVTransform(const Matrix4x4& uvTransform);
    void SetLightingMode(int32_t lightingMode);
    void SetTextureFilePath(const std::string& textureFilePath);
    Vector4 GetColor() const;
    float GetAlphaReference() const;
    int32_t GetLightingMode() const;
    const std::string& GetTextureFilePath() const;
    const Material& GetMaterial() const { return *materialData_; }
    const Animation& GetAnimation() const { return modelData_.animation; }
    const Node& GetRootNode() const { return modelData_.rootNode; }
    const SkinClusterData& GetSkinClusterData() const { return modelData_.skinClusterData; }
    bool HasAnimation() const
    {
        return !modelData_.animation.nodeAnimations.empty();
    }
    bool HasSkinCluster() const
    {
        return !modelData_.skinClusterData.jointWeights.empty();
    }

    // Object3dとCompute Skinningが使用するGPUビューおよびCPU側データを借用して返す。
    const D3D12_VERTEX_BUFFER_VIEW& GetVBV() const { return vertexBufferView_; }
    const D3D12_INDEX_BUFFER_VIEW& GetIBV() const { return indexBufferView_; }
    ID3D12Resource* GetMaterialResource() const { return materialResource_.Get(); }
    size_t GetVertexCount() const { return modelData_.vertices.size(); }
    size_t GetIndexCount() const { return modelData_.indices.size(); }
    const std::vector<VertexData>& GetVertices() const { return modelData_.vertices; }

    // ファイル読込、アニメーション計算、組み込みプリミティブ生成を行う共有処理。
    static MaterialData LoadMaterialTemplate(const std::string& directoryPath, const std::string& filename);
    static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
    static ModelData LoadAssimpFile(const std::string& directoryPath, const std::string& filename);
    static QuaternionTransform CalculateValue(const NodeAnimation& nodeAnimation, float time);
    static Skeleton CreateSkeleton(const Node& rootNode);
    static void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float time);
    static void UpdateSkeleton(Skeleton& skeleton);
    static ModelData CreateTriangleData(float width, float height, const std::string& textureFilePath);
    static ModelData CreatePlaneData(float width, float height, const std::string& textureFilePath);
    static ModelData CreateCircleData(
        uint32_t divideCount,
        float radius,
        const std::string& textureFilePath);
    static ModelData CreateHeartData(
        uint32_t divideCount,
        float radius,
        const std::string& textureFilePath);
    static ModelData CreateRingData(
        uint32_t divideCount,
        float outerRadius,
        float innerRadius,
        const std::string& textureFilePath);
    static ModelData CreateSphereData(
        uint32_t latDivideCount,
        uint32_t lonDivideCount,
        float radius,
        const std::string& textureFilePath);
    static ModelData CreateTorusData(
        uint32_t majorDivideCount,
        uint32_t minorDivideCount,
        float majorRadius,
        float minorRadius,
        const std::string& textureFilePath);
    static ModelData CreateCylinderData(
        uint32_t divideCount,
        float topRadius,
        float bottomRadius,
        float height,
        const std::string& textureFilePath);
    static ModelData CreateConeData(
        uint32_t divideCount,
        float radius,
        float height,
        const std::string& textureFilePath);
    static ModelData CreateBoxData(
        float width,
        float height,
        float depth,
        const std::string& textureFilePath);

private:
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateMaterial();

private:
    ModelCommon* modelCommon_ = nullptr; // 描画基盤を参照する借用ポインタ

    ModelData modelData_; // ファイルから読み込んだCPU側モデル情報

    ComPtr<ID3D12Resource> vertexResource_;       // GPUへ渡す頂点バッファ
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{}; // 頂点バッファの描画ビュー
    ComPtr<ID3D12Resource> indexResource_;        // GPUへ渡すインデックスバッファ
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};   // インデックスバッファの描画ビュー

    ComPtr<ID3D12Resource> materialResource_; // GPUへ渡すマテリアル定数バッファ
    Material* materialData_ = nullptr;        // materialResource_のCPU書込み先
};
