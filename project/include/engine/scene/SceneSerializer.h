#pragma once
#include <string>
#include <vector>
#include "engine/base/Math.h"

// エディタ用シーンJSONとC++データの相互変換を担当する。
// ゲーム固有クラスを保存せず、復元可能な描画・変換パラメータだけを扱う。
namespace SceneSerializer {

// 1オブジェクト分の永続化データ。GPU資源や実行時ポインタは含まない。
struct ObjectRecord {
    std::string name;                                    // エディター上の識別名
    bool primitive = false;                              // 組み込み形状ならtrue
    int modelIndex = -1;                                 // エディターのモデル一覧番号
    Math::Vector3 translate{};                            // ワールド位置
    Math::Vector3 rotate{};                               // XYZ軸の回転角（ラジアン）
    Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };             // 各軸の拡大率
    Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };       // マテリアルへ乗算するRGBA
    float alphaReference = 0.0f;                         // アルファカットアウトのしきい値
    int lightingMode = 2;                                // Object3dのライティング方式番号
    std::string textureFilePath;                          // 個別に上書きするテクスチャパス
};

// シーン全体で共有するカメラとライトの永続化データ。
// hasCamera / hasLightingがfalseなら、読み込み側は現在値を維持できる。
struct SceneSettings {
    bool hasCamera = false;                                  // カメラ値がファイルに含まれるか
    Math::Vector3 cameraTranslate{};                          // カメラのワールド位置
    Math::Vector3 cameraRotate{};                             // カメラのXYZ回転角
    bool hasLighting = false;                                // ライト値がファイルに含まれるか
    Math::Vector3 lightDirection{ 0.0f, -1.0f, 0.0f };       // 平行光源が進む方向
    float lightIntensity = 1.0f;                             // 平行光源の光量
    Math::Vector3 pointLightPosition{ 0.0f, 2.0f, 0.0f };    // 点光源の位置
    float pointLightIntensity = 1.0f;                        // 点光源の光量
    Math::Vector3 spotLightPosition{ 2.0f, 1.25f, 0.0f };    // スポットライトの位置
    Math::Vector3 spotLightDirection{ -1.0f, 1.0f, 0.0f };  // スポットライトが進む方向
    float spotLightIntensity = 4.0f;                         // スポットライトの光量
};

// オブジェクトだけを保存・読込する簡易API。内部ではシーンAPIを利用する。
bool SaveObjects(
    const char* path,
    const std::vector<ObjectRecord>& records);
bool LoadObjects(
    const char* path,
    std::vector<ObjectRecord>& records);
// 完全なシーンを保存・読込する。成功時true、パス不正やファイル失敗時false。
bool SaveScene(
    const char* path,
    const std::vector<ObjectRecord>& records,
    const SceneSettings& settings);
bool LoadScene(
    const char* path,
    std::vector<ObjectRecord>& records,
    SceneSettings& settings);
// records内の要素を借用して返す。vectorを変更した後は戻り値を保持しないこと。
const ObjectRecord* FindObjectByName(
    const std::vector<ObjectRecord>& records,
    const char* name);

} // namespace SceneSerializer
