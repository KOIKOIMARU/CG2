#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>

#include "engine/3d/Object3d.h" // ★追加（Object3dを持つため）

class Object3dCommon;          // ★前方宣言でOK

class Stage {
public:
    static constexpr float kTileSize = 1.0f;
    static constexpr int kMapW = 20;
    static constexpr int kMapH = 12;

    using MapRow = std::array<int, kMapW>;
    using Map = std::array<MapRow, kMapH>;

    Stage();

    void SetMap(const Map& map) { map_ = map; }

    // ★描画用
    void Initialize(Object3dCommon* objCommon);  // タイルObject生成
    void Draw();                                 // タイルObject描画

    bool IsSolidTileByIndex(int mapX, int mapY) const;
    bool IsSolidAtWorld(float wx, float wy) const;

    int  GetTileByIndex(int mapX, int mapY) const { return map_[mapY][mapX]; }
    const Map& GetMap() const { return map_; }

    static int WorldToTileX(float wx);
    static int WorldToTileYWorld(float wy);
    static int TileYWorldToMapY(int tileYWorld) { return kMapH - 1 - tileYWorld; }

private:
    Map map_{};

    // ★描画用
    Object3dCommon* objCommon_ = nullptr;
    std::vector<Object3d> blockObjs_;
};
