#include "Stage.h"

Stage::Stage() {
    // あなたの gMap をそのまま移植（同じ並び）
    map_ = { {
            // ======= 天井 =======
            MapRow{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},

            // ======= 空間 =======
            MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},

            // ======= 上段足場 =======
            MapRow{1,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1},

            // ======= 空間 =======
            MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},

            // ======= 中段足場 =======
            MapRow{1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1},

            // ======= 空間 =======
            MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},

            // ======= 床 =======
            MapRow{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        } };
}

bool Stage::IsSolidTileByIndex(int mapX, int mapY) const {
    // マップ外は壁扱い（今の挙動そのまま）
    if (mapX < 0 || mapX >= kMapW || mapY < 0 || mapY >= kMapH) {
        return true;
    }
    return map_[mapY][mapX] == 1;
}

int Stage::WorldToTileX(float wx) {
    return static_cast<int>(std::floor(wx / kTileSize));
}

int Stage::WorldToTileYWorld(float wy) {
    return static_cast<int>(std::floor(wy / kTileSize)); // 下から数える
}

bool Stage::IsSolidAtWorld(float wx, float wy) const {
    const int tileX = WorldToTileX(wx);
    const int tileYWorld = WorldToTileYWorld(wy);
    const int mapY = TileYWorldToMapY(tileYWorld);
    return IsSolidTileByIndex(tileX, mapY);
}
