#pragma once
#include "engine/base/DirectXCommon.h"

class SrvManager;

class ModelCommon
{
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    SrvManager* GetSrvManager() const { return srvManager_; }

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
};
