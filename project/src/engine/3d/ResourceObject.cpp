#include "engine/3d/ResourceObject.h"

ResourceObject::ResourceObject(ID3D12Resource* resource)
    : resource_(resource) {
    // 渡されたCOM参照をそのまま所有する。ここではAddRefしない。
}

ResourceObject::~ResourceObject() {
    // 所有している参照はこの場所だけで解放する。
    if (resource_) {
        resource_->Release();
    }
}

ID3D12Resource* ResourceObject::Get() const {
    return resource_;
}
