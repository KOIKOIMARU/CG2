#pragma once

#include "engine/base/Math.h"

#include <string>
#include <vector>

namespace LevelDataLoader {

struct ColliderData {
    bool enabled = false;
    std::string type;
    Math::Vector3 center{};
    Math::Vector3 size{};
};

struct ObjectData {
    bool disabled = false;
    std::string type;
    std::string name;
    std::string fileName;
    Math::Vector3 translation{};
    Math::Vector3 rotation{};
    Math::Vector3 scaling{ 1.0f, 1.0f, 1.0f };
    ColliderData collider{};
    std::vector<ObjectData> children;
};

struct PlayerSpawnData {
    Math::Vector3 translation{};
    Math::Vector3 rotation{};
};

struct EnemySpawnData {
    std::string fileName;
    Math::Vector3 translation{};
    Math::Vector3 rotation{};
};

struct CameraData {
    Math::Vector3 translation{};
    Math::Vector3 rotation{};
};

struct LevelData {
    std::string name;
    std::vector<ObjectData> objects;
    std::vector<PlayerSpawnData> players;
    std::vector<EnemySpawnData> enemies;
    std::vector<CameraData> cameras;
};

// Blenderアドオンが出力したレベルJSONを読み込む。
// TransformとコライダーはBlender座標系からゲーム座標系へ変換される。
bool LoadFile(
    const char* path,
    LevelData& levelData,
    std::string* errorMessage = nullptr);

} // namespace LevelDataLoader
