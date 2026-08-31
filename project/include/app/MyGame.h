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
    bool enabled = false;
    double gameplaySeconds = 15.0;
    double startupTimeoutSeconds = 120.0;
    std::filesystem::path logPath;
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

    std::unique_ptr<AbstractSceneFactory> sceneFactory_;
    SmokeTestOptions smokeTestOptions_;
    std::chrono::steady_clock::time_point smokeTestStartTime_{};
    std::chrono::steady_clock::time_point smokeGameplayStartTime_{};
    bool smokeAutoStartRequested_ = false;
    bool smokeGameplayStarted_ = false;
    bool smokeTestFinished_ = false;
    uint64_t smokeGameplayFrameCount_ = 0;
};
