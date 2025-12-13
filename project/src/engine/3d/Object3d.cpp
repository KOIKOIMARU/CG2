#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include <cassert>

void Object3d::Initialize(Object3dCommon* object3dCommon) {
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;
}
