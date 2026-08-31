#include "engine/3d/ModelCommon.h"
#include "engine/base/SrvManager.h"

void ModelCommon::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager)
{
    // 所有権は移さず、モデル群が共有する基盤への参照だけを保持する。
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
}
