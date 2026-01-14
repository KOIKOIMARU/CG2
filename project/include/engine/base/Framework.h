#pragma once

class Framework {
public:
    virtual ~Framework() = default;

    // WinMainから呼ぶのはこれだけ
    void Run();

protected:
    // 派生(MyGame)側で実装
    virtual void Initialize() = 0;
    virtual void Finalize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;

    bool isEndRequest_ = false;
};
