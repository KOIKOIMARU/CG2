#include "engine/scene/world/Stage.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/ModelManager.h"
#include <cassert>

Stage::Stage() {
    map_ = { {
        MapRow{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        MapRow{1,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1},
        MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        MapRow{1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1},
        MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        MapRow{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        MapRow{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    } };
}

void Stage::Initialize(Object3dCommon* objCommon) {
    objCommon_ = objCommon;

    // WinMainで LoadModel 済み前提
    auto* blockModel = ModelManager::GetInstance()->FindModel("block.obj");
    assert(blockModel);

    blockObjs_.clear();
    blockObjs_.reserve(kMapW * kMapH);

    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            if (map_[y][x] != 1) { continue; }

            blockObjs_.emplace_back();
            auto& obj = blockObjs_.back();

            obj.Initialize(objCommon_);
            obj.SetModel(blockModel);
            obj.SetEnableLighting(0);


            const float wx = (x + 0.5f) * kTileSize;
            const int tileYWorld = (kMapH - 1) - y;
            const float wy = (tileYWorld + 0.5f) * kTileSize;

            obj.SetTranslate({ wx, wy, 0.0f });
            obj.SetScale({ kTileSize, kTileSize, kTileSize });

            // ★これ必須寄り
            obj.Update();
        }
    }
}


void Stage::Draw() {
    for (auto& obj : blockObjs_) {
        obj.Draw();
    }
}


bool Stage::IsSolidTileByIndex(int mapX, int mapY) const {
    if (mapX < 0 || mapX >= kMapW || mapY < 0 || mapY >= kMapH) {
        return true;
    }
    return map_[mapY][mapX] == 1;
}

int Stage::WorldToTileX(float wx) {
    return static_cast<int>(std::floor(wx / kTileSize));
}

int Stage::WorldToTileYWorld(float wy) {
    return static_cast<int>(std::floor(wy / kTileSize));
}

bool Stage::IsSolidAtWorld(float wx, float wy) const {
    const int tileX = WorldToTileX(wx);
    const int tileYWorld = WorldToTileYWorld(wy);
    const int mapY = TileYWorldToMapY(tileYWorld);
    return IsSolidTileByIndex(tileX, mapY);
}
