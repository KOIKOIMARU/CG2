#pragma once
#include "engine/base/DirectXCommon.h"
#include <string>

class SrvManager;

// Modelが共通利用するDirectX 12基盤とSRV管理への窓口。
// このクラスは各システムを所有せず、Frameworkの生存期間中だけ借用する。
class ModelCommon
{
public:
    // モデル生成より先に一度だけ呼び出す。
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    SrvManager* GetSrvManager() const { return srvManager_; }
    // PBR描画で使用する既定の環境マップ。空文字列なら環境反射を使用しない。
    void SetEnvironmentTexturePath(const std::string& texturePath) {
        environmentTexturePath_ = texturePath;
    }
    const std::string& GetEnvironmentTexturePath() const {
        return environmentTexturePath_;
    }

private:
    DirectXCommon* dxCommon_ = nullptr; // デバイスとコマンドリストの借用先
    SrvManager* srvManager_ = nullptr;  // モデル用SRVを登録する借用先
    std::string environmentTexturePath_; // 全モデルが既定で使う環境マップ
};
