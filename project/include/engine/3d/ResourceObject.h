#ifndef RESOURCEOBJECT_H
#define RESOURCEOBJECT_H

#include <d3d12.h>

// ID3D12Resourceの参照を1つ所有し、スコープ終了時にReleaseする小さなRAIIラッパー。
// コンストラクタへ渡した参照は本クラスへ移譲されるため、呼び出し側で重ねてReleaseしない。
class ResourceObject {
public:
    explicit ResourceObject(ID3D12Resource* resource);
    ~ResourceObject();

    // 非所有ポインタを返す。ResourceObjectより長く保持しないこと。
    ID3D12Resource* Get() const;

private:
    ID3D12Resource* resource_;
};

#endif // RESOURCEOBJECT_H
