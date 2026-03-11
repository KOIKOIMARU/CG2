#pragma once
#include <memory>

class BaseScene;

class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager();

    void Update();
    void Draw();

    void SetNextScene(std::unique_ptr<BaseScene> nextScene);

private:
    std::unique_ptr<BaseScene> scene_;
    std::unique_ptr<BaseScene> nextScene_;
};