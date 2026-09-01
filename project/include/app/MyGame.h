#pragma once
#include "engine/base/Framework.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

class AbstractSceneFactory;

// 自動実行検証に使用する設定。通常起動ではenabledをfalseにする。
struct SmokeTestOptions {
    bool enabled = false;                    // 自動起動・終了判定を有効にするか
    double gameplaySeconds = 15.0;           // Play開始後に継続して検証する秒数
    double startupTimeoutSeconds = 120.0;    // Play開始を待てる最大秒数
    std::filesystem::path logPath;            // 結果を書き出すログファイル
};

// 実行ファイル側の構成ルート。Frameworkへシーン生成規則を接続する。
class MyGame : public Framework {
public:
    explicit MyGame(SmokeTestOptions smokeTestOptions = {});
    ~MyGame() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

private:
    void UpdateSmokeTest();
    void FailSmokeTest(std::string_view reason, int exitCode);
    void WriteSmokeLog(std::string_view message) const;

    std::unique_ptr<AbstractSceneFactory> sceneFactory_; // シーン生成規則の所有先
    SmokeTestOptions smokeTestOptions_;                  // 起動時に渡された自動検証条件
    std::chrono::steady_clock::time_point smokeTestStartTime_{}; // アプリ起動時刻
    std::chrono::steady_clock::time_point smokeGameplayStartTime_{}; // Play開始時刻
    bool smokeGameplayStarted_ = false;                  // Playモード開始を確認したか
    bool smokeTestFinished_ = false;                     // 結果確定後の重複処理防止
    uint64_t smokeGameplayFrameCount_ = 0;               // Play中に描画できたフレーム数
};
