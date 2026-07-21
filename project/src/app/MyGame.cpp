#include "app/MyGame.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneFactory.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/base/ImGuiManager.h"
#include "engine/3d/ModelManager.h"
#include "engine/io/Input.h"
#include "engine/scene/EditorScene.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/SceneType.h"
#include "engine/scene/TitleScene.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace {

float ToMilliseconds(
    const std::chrono::steady_clock::time_point& begin,
    const std::chrono::steady_clock::time_point& end)
{
    return std::chrono::duration<float, std::milli>(end - begin).count();
}

}

MyGame::MyGame(SmokeTestOptions smokeTestOptions)
    : smokeTestOptions_(std::move(smokeTestOptions))
{
    if (smokeTestOptions_.enabled && smokeTestOptions_.logPath.empty()) {
        smokeTestOptions_.logPath = "smoke-test.log";
    }
}
MyGame::~MyGame() = default;

void MyGame::Initialize() {
    if (smokeTestOptions_.enabled) {
        smokeTestStartTime_ = std::chrono::steady_clock::now();

        const auto parentPath = smokeTestOptions_.logPath.parent_path();
        if (!parentPath.empty()) {
            std::error_code error;
            std::filesystem::create_directories(parentPath, error);
        }

        std::ofstream clearLog(
            smokeTestOptions_.logPath,
            std::ios::out | std::ios::trunc);
        clearLog.close();

        std::ostringstream message;
        message << "SMOKE_TEST_START gameplay_seconds="
                << smokeTestOptions_.gameplaySeconds
                << " startup_timeout_seconds="
                << smokeTestOptions_.startupTimeoutSeconds;
        WriteSmokeLog(message.str());
    }

    Framework::Initialize();

    if (smokeTestOptions_.enabled) {
        WriteSmokeLog("SMOKE_TEST_ENGINE_INITIALIZED");
    }

    sceneFactory_ = std::make_unique<SceneFactory>();

    SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
    SceneManager::GetInstance()->SetSystems(
        dxCommon_.get(),
        srvManager_.get(),
        spriteCommon_.get(),
        imguiManager_.get(),
        input_.get()
    );

    SceneManager::GetInstance()->SetNextScene(
        smokeTestOptions_.enabled ?
            SceneType::Title :
            SceneType::BonusShowcase);
}

void MyGame::Update() {
    const auto updateBegin = std::chrono::steady_clock::now();
    Framework::Update();
    if (endRequst_) {
        if (smokeTestOptions_.enabled && !smokeTestFinished_) {
            FailSmokeTest("window_closed_before_completion", 2);
        }
        if (dxCommon_) {
            dxCommon_->EditFrameTiming().updateMs =
                ToMilliseconds(updateBegin, std::chrono::steady_clock::now());
        }
        return;
    }

    imguiManager_->Begin();

    SceneManager::GetInstance()->Update();

    imguiManager_->End();

    UpdateSmokeTest();

    dxCommon_->EditFrameTiming().updateMs =
        ToMilliseconds(updateBegin, std::chrono::steady_clock::now());
}

void MyGame::Draw() {
    auto& timing = dxCommon_->EditFrameTiming();
    const auto drawBegin = std::chrono::steady_clock::now();
    auto sectionBegin = drawBegin;

    dxCommon_->PreDraw();
    srvManager_->PreDraw();
    timing.preDrawMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    sectionBegin = std::chrono::steady_clock::now();
    SceneManager::GetInstance()->Draw();
    timing.sceneDrawMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    int postEffectMode = 0;
    if (auto* editorScene =
            dynamic_cast<EditorScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
        postEffectMode = editorScene->GetPostEffectMode();
        dxCommon_->SetPostEffectProjectionMatrix(editorScene->GetProjectionMatrix());
    } else if (auto* gameScene =
            dynamic_cast<GameScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
        postEffectMode = gameScene->GetPostEffectMode();
        dxCommon_->SetPostEffectProjectionMatrix(gameScene->GetProjectionMatrix());
    }

    sectionBegin = std::chrono::steady_clock::now();
    dxCommon_->DrawRenderTextureToSwapChain(postEffectMode);
    timing.postEffectMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    sectionBegin = std::chrono::steady_clock::now();
    imguiManager_->Draw();
    timing.imguiDrawMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    dxCommon_->PostDraw();
    timing.frameCpuMs =
        timing.updateMs +
        ToMilliseconds(drawBegin, std::chrono::steady_clock::now());
}

void MyGame::Finalize() {
    SceneManager::GetInstance()->FinalizeCurrentScene();
    ModelManager::GetInstance()->Finalize();
    sceneFactory_.reset();
    Framework::Finalize();

    if (smokeTestOptions_.enabled) {
        if (!smokeTestFinished_) {
            WriteSmokeLog("SMOKE_TEST_FAIL reason=finalized_before_completion");
            exitCode_ = 3;
        }

        std::ostringstream message;
        message << "SMOKE_TEST_END exit_code=" << exitCode_;
        WriteSmokeLog(message.str());
    }
}

void MyGame::UpdateSmokeTest()
{
    if (!smokeTestOptions_.enabled || smokeTestFinished_) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    auto* sceneManager = SceneManager::GetInstance();

    if (!smokeGameplayStarted_) {
        if (dynamic_cast<GameScene*>(sceneManager->GetCurrentScene())) {
            smokeGameplayStarted_ = true;
            smokeGameplayStartTime_ = now;
            WriteSmokeLog("SMOKE_TEST_GAMEPLAY_ENTERED");
        } else if (!smokeAutoStartRequested_ &&
                   dynamic_cast<TitleScene*>(sceneManager->GetCurrentScene()) &&
                   sceneManager->IsScenePrepared(SceneType::Game)) {
            smokeAutoStartRequested_ = true;
            sceneManager->SetNextScene(SceneType::Game);
            WriteSmokeLog("SMOKE_TEST_AUTO_START_REQUESTED");
        }

        const double startupElapsedSeconds =
            std::chrono::duration<double>(now - smokeTestStartTime_).count();
        if (!smokeGameplayStarted_ &&
            startupElapsedSeconds >= smokeTestOptions_.startupTimeoutSeconds) {
            FailSmokeTest("gameplay_start_timeout", 4);
        }
        return;
    }

    if (!dynamic_cast<GameScene*>(sceneManager->GetCurrentScene())) {
        FailSmokeTest("gameplay_scene_exited_early", 5);
        return;
    }

    ++smokeGameplayFrameCount_;
    const double gameplayElapsedSeconds =
        std::chrono::duration<double>(now - smokeGameplayStartTime_).count();
    if (gameplayElapsedSeconds >= smokeTestOptions_.gameplaySeconds) {
        std::ostringstream message;
        message << "SMOKE_TEST_PASS gameplay_elapsed_seconds="
                << std::fixed << std::setprecision(3)
                << gameplayElapsedSeconds
                << " monitored_frames=" << smokeGameplayFrameCount_;
        WriteSmokeLog(message.str());

        smokeTestFinished_ = true;
        exitCode_ = 0;
        endRequst_ = true;
    }
}

void MyGame::FailSmokeTest(std::string_view reason, int exitCode)
{
    if (smokeTestFinished_) {
        return;
    }

    std::ostringstream message;
    message << "SMOKE_TEST_FAIL reason=" << reason;
    WriteSmokeLog(message.str());

    smokeTestFinished_ = true;
    exitCode_ = exitCode;
    endRequst_ = true;
}

void MyGame::WriteSmokeLog(std::string_view message) const
{
    if (!smokeTestOptions_.enabled || smokeTestOptions_.logPath.empty()) {
        return;
    }

    double elapsedSeconds = 0.0;
    if (smokeTestStartTime_.time_since_epoch().count() != 0) {
        elapsedSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - smokeTestStartTime_).count();
    }

    std::ofstream log(smokeTestOptions_.logPath, std::ios::out | std::ios::app);
    if (!log) {
        return;
    }

    log << '[' << std::fixed << std::setprecision(3)
        << elapsedSeconds << "s] " << message << '\n';
}
