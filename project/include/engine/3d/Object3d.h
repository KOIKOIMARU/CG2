#pragma once
class Object3dCommon;

class Object3d {
public:
    void Initialize(Object3dCommon* object3dCommon);
    void Update();
    void Draw();


private:
    Object3dCommon* object3dCommon_ = nullptr;
};
