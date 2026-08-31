#pragma once
#include <string>
#include <vector>
#include "engine/base/Math.h"

// エディタ用シーンJSONとC++データの相互変換を担当する。
// ゲーム固有クラスを保存せず、復元可能な描画・変換パラメータだけを扱う。
namespace SceneSerializer {

// 1オブジェクト分の永続化データ。GPU資源や実行時ポインタは含まない。
struct ObjectRecord {
    std::string name;
    bool primitive = false;
    int modelIndex = -1;
    Math::Vector3 translate{};
    Math::Vector3 rotate{};
    Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };
    Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    float alphaReference = 0.0f;
    int lightingMode = 2;
    std::string textureFilePath;
};

// シーン全体で共有するカメラとライトの永続化データ。
// hasCamera / hasLightingがfalseなら、読み込み側は現在値を維持できる。
struct SceneSettings {
    bool hasCamera = false;
    Math::Vector3 cameraTranslate{};
    Math::Vector3 cameraRotate{};
    bool hasLighting = false;
    Math::Vector3 lightDirection{ 0.0f, -1.0f, 0.0f };
    float lightIntensity = 1.0f;
    Math::Vector3 pointLightPosition{ 0.0f, 2.0f, 0.0f };
    float pointLightIntensity = 1.0f;
    Math::Vector3 spotLightPosition{ 2.0f, 1.25f, 0.0f };
    Math::Vector3 spotLightDirection{ -1.0f, 1.0f, 0.0f };
    float spotLightIntensity = 4.0f;
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
