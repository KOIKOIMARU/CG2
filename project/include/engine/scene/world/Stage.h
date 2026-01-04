#pragma once
#include <array>
#include <cmath>
#include <cstdint>

class Stage {
public:
    static constexpr float kTileSize = 1.0f;
    static constexpr int kMapW = 20;
    static constexpr int kMapH = 12;

    using MapRow = std::array<int, kMapW>;
    using Map = std::array<MapRow, kMapH>;

    Stage();

    // そのまま差し替えたい場合用（固定マップでOKなら使わなくてもいい）
    void SetMap(const Map& map) { map_ = map; }

    // ---- 当たり判定 ----
    // mapX,mapY は gMap のインデックス（上が0、下がkMapH-1）を想定
    bool IsSolidTileByIndex(int mapX, int mapY) const;

    // ワールド座標(wx,wy)が含まれるタイルがSolidか
    bool IsSolidAtWorld(float wx, float wy) const;

    // デバッグや描画用に取り出したい時
    int  GetTileByIndex(int mapX, int mapY) const { return map_[mapY][mapX]; }
    const Map& GetMap() const { return map_; }

    // 便利：ワールド→(tileX, tileYWorld) 変換（tileYWorldは“下から数える”）
    static int WorldToTileX(float wx);
    static int WorldToTileYWorld(float wy);

    // tileYWorld（下から）→ mapY（gMap配列用：上が0）
    static int TileYWorldToMapY(int tileYWorld) { return kMapH - 1 - tileYWorld; }

private:
    Map map_{};
};
