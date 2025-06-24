#ifndef RESOURCEOBJECT_H
#define RESOURCEOBJECT_H

#include <d3d12.h>

class ResourceObject {
public:
    explicit ResourceObject(ID3D12Resource* resource);
    ~ResourceObject();

    ID3D12Resource* Get() const;

private:
    ID3D12Resource* resource_;
};

#endif // RESOURCEOBJECT_H
