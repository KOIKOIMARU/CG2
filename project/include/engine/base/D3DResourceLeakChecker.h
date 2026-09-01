#pragma once

// Debugビルドの終了時に、解放されず残ったDXGI/D3D12資源を出力する。
class D3DResourceLeakChecker
{
public:
    ~D3DResourceLeakChecker();
};
