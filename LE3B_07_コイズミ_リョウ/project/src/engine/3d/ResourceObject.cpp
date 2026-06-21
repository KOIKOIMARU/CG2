#include "engine/3d/ResourceObject.h"

ResourceObject::ResourceObject(ID3D12Resource* resource)
    : resource_(resource) {
}

ResourceObject::~ResourceObject() {
    if (resource_) {
        resource_->Release();
    }
}

ID3D12Resource* ResourceObject::Get() const {
    return resource_;
}
