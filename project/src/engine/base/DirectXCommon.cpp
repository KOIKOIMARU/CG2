#include "engine/base/DirectXCommon.h"
#include "engine/base/Logger.h"
#include <cassert>
#include <format>
#include <dxcapi.h>            // DXC（dxcUtils, dxcCompiler, IncludeHandler）
#include <vector>              // arguments 用
#include <string>              // wstring
#include <chrono>
#include <thread>   // sleep_for 用



#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include <d3dx12.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
using namespace Microsoft::WRL;
using Logger::Log;


void DirectXCommon::Initialize(WinApp* winApp)
{
    // WinAppを覚えておく
    assert(winApp);
    this->winApp_ = winApp;

    InitializeDevice();      // デバイス・DXGI
    InitializeCommand();     // コマンドキュー/アロケータ/リスト
    InitializeSwapChain();   // ★ スワップチェーン
    InitializeDepthBuffer();   // ★ 深度バッフ
    InitializeDescriptorHeaps();
    InitializeRenderTargetView();
    InitializeDepthStencilView();
    InitializeFence();
    InitializeViewport();       // ★ここで初期化
    InitializeScissorRect();
    InitializeDXC();
    InitializeImGui();

    // ★ FPS 固定の初期化
    InitializeFixFPS();
}


void DirectXCommon::PreDraw() {

    // 0. コマンドリストを描画用に準備
    HRESULT hr = S_OK;
    hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));

    // バックバッファの番号取得
    UINT bbIndex = swapChain_->GetCurrentBackBufferIndex();

    // 2. TransitionBarrier PRESENT → RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[bbIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    // 3. RTV & DSV の設定
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += bbIndex * rtvDescriptorSize_;

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
        dsvHeap_->GetCPUDescriptorHandleForHeapStart();

    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // 4. クリア
    float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList_->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr
    );

    // 5. ビューポート & シザー
    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);

    // 6. SRV ヒープ設定
    ID3D12DescriptorHeap* heaps[] = { srvHeap_.Get() };
    commandList_->SetDescriptorHeaps(_countof(heaps), heaps);
}


void DirectXCommon::PostDraw()
{
    HRESULT hr = S_OK;

    // バックバッファの番号取得
    UINT bbIndex = swapChain_->GetCurrentBackBufferIndex();

    // RenderTarget → Present へのリソースバリア
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[bbIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    // コマンドリストをクローズ
    hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    // GPU コマンドの実行
    ID3D12CommandList* cmdLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // 画面のフリップ（Present）
    hr = swapChain_->Present(1, 0);
    assert(SUCCEEDED(hr));

    // Fence の値更新 & Signal
    fenceValue_++;
    hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
    assert(SUCCEEDED(hr));

    // コマンド完了待ち
    if (fence_->GetCompletedValue() < fenceValue_) {
        hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        assert(SUCCEEDED(hr));
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    // ★ ここで FPS 固定
    UpdateFixFPS();


}

void DirectXCommon::InitializeDevice() {

#ifdef _DEBUG
    ComPtr<ID3D12Debug1> debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(true);
    }
#endif

    // DXGIファクトリーの生成（★ メンバに直接作る）
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    // 使用するアダプタ用の変数
    ComPtr<IDXGIAdapter4> useAdapter = nullptr;
    // 良い順にアダプタを探す
    for (UINT i = 0;
        dxgiFactory_->EnumAdapterByGpuPreference(
            i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
        ++i) {

        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr));

        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            break;
        }
        useAdapter = nullptr;
    }
    assert(useAdapter != nullptr);

    // ★ 一時変数じゃなくてそのまま device_ に作る
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0
    };
    const char* featureLevelStrings[] = { "12.2","12.1","12.0" };

    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        hr = D3D12CreateDevice(
            useAdapter.Get(),
            featureLevels[i],
            IID_PPV_ARGS(&device_));
        if (SUCCEEDED(hr)) {
            Log(std::format("Feature Level: {}\n", featureLevelStrings[i]));
            break;
        }
    }
    assert(device_ != nullptr);
    Log("Complete create D3D12Device!!!\n");

#ifdef _DEBUG
    ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

        D3D12_MESSAGE_ID denyIds[] = {
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
        };
        D3D12_MESSAGE_SEVERITY severities[] = {
            D3D12_MESSAGE_SEVERITY_INFO
        };
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        infoQueue->PushStorageFilter(&filter);
    }
#endif
}

void DirectXCommon::InitializeCommand()
{
    assert(device_);  // 念のため

    HRESULT hr{};

    // コマンドキュー生成
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    hr = device_->CreateCommandQueue(
        &commandQueueDesc,
        IID_PPV_ARGS(&commandQueue_));
    assert(SUCCEEDED(hr));

    // コマンドアロケータ生成
    hr = device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator_));
    assert(SUCCEEDED(hr));

    // コマンドリスト生成
    hr = device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator_.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList_));
    assert(SUCCEEDED(hr));

    // いったん Close しておく（毎フレーム Reset して使う想定）
    hr = commandList_->Close();
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeSwapChain()
{
    assert(dxgiFactory_);
    assert(commandQueue_);
    assert(winApp_);

    HRESULT hr{};

    // スワップチェーンの設定
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = WinApp::kClientWidth;   // 画面の幅
    swapChainDesc.Height = WinApp::kClientHeight;  // 画面の高さ
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   // 色の形式
    swapChainDesc.SampleDesc.Count = 1;                       // マルチサンプルなし
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;                       // ダブルバッファ
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

    // 一旦 IDXGISwapChain1 として作成
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1 = nullptr;
    hr = dxgiFactory_->CreateSwapChainForHwnd(
        commandQueue_.Get(),          // コマンドキュー
        winApp_->GetHwnd(),           // ウィンドウハンドル
        &swapChainDesc,               // 設定
        nullptr,                      // フルスクリーンの設定（今回は使わない）
        nullptr,                      // 出力先モニタ
        &swapChain1);                 // 返ってくるスワップチェーン
    assert(SUCCEEDED(hr));

    // IDXGISwapChain4 にキャストしてメンバに保持
    hr = swapChain1.As(&swapChain_);
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeDepthBuffer()
{
    assert(device_);  // 念のため

    // 生成するResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = WinApp::kClientWidth;   // 画面サイズに合わせる
    resourceDesc.Height = WinApp::kClientHeight;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;         // 深度24bit + ステンシル8bit
    resourceDesc.SampleDesc.Count = 1;                                      // MSAAなし
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    // ヒーププロパティ
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    // 初期クリア値
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.DepthStencil.Stencil = 0;

    // リソース生成（★ 結果だけメンバに持つ）
    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(&depthStencilResource_));
    assert(SUCCEEDED(hr));
}

ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(
    D3D12_DESCRIPTOR_HEAP_TYPE heapType,
    UINT numDescriptors,
    bool shaderVisible)
{
    assert(device_);

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = heapType;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = shaderVisible
        ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 0;

    ComPtr<ID3D12DescriptorHeap> heap;
    HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
    assert(SUCCEEDED(hr));

    return heap;
}

void DirectXCommon::InitializeDescriptorHeaps()
{
    assert(device_);

    // 各ディスクリプタサイズを取得しておく
    rtvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    srvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    dsvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // ヒープを生成（個数は今まで main.cpp で使っていた数に合わせる）
    // 足りなくなったら適宜増やしてOK
    rtvHeap_ = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);          // バックバッファ2枚分
    srvHeap_ = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true); // テクスチャやCBV用
    dsvHeap_ = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);          // 深度1枚
}

void DirectXCommon::InitializeRenderTargetView()
{
    assert(device_);
    assert(swapChain_);
    assert(rtvHeap_);

    for (UINT i = 0; i < kBackBufferCount; ++i) {
        // スワップチェーンからバックバッファのリソースを取得
        HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
        assert(SUCCEEDED(hr));

        // 対応するRTVハンドルを計算
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
            GetCPUDescriptorHandle(rtvHeap_, rtvDescriptorSize_, i);

        // RTVを生成
        device_->CreateRenderTargetView(swapChainResources_[i].Get(), nullptr, rtvHandle);
    }
}


D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(
    const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
    uint32_t descriptorSize, uint32_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(
    const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
    uint32_t descriptorSize, uint32_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle =
        descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(descriptorSize) * index;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index)
{
    assert(srvHeap_);
    return GetCPUDescriptorHandle(srvHeap_, srvDescriptorSize_, index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index)
{
    assert(srvHeap_);
    return GetGPUDescriptorHandle(srvHeap_, srvDescriptorSize_, index);
}

void DirectXCommon::InitializeDepthStencilView()
{
    assert(device_);
    assert(depthStencilResource_);
    assert(dsvHeap_);

    // DSV の設定
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    // DSV を DSVヒープの先頭に 1 個だけ作成
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
        dsvHeap_->GetCPUDescriptorHandleForHeapStart();

    device_->CreateDepthStencilView(
        depthStencilResource_.Get(),
        &dsvDesc,
        dsvHandle);
}

void DirectXCommon::InitializeFence()
{
    assert(device_);

    // フェンス生成
    HRESULT hr = device_->CreateFence(
        fenceValue_,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));

    // イベント生成（待機に使う）
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);
}

void DirectXCommon::InitializeViewport()
{
    viewport_.Width = static_cast<float>(WinApp::kClientWidth);
    viewport_.Height = static_cast<float>(WinApp::kClientHeight);
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
}

void DirectXCommon::InitializeScissorRect()
{
    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kClientWidth;
    scissorRect_.bottom = WinApp::kClientHeight;
}

void DirectXCommon::InitializeDXC()
{
    HRESULT hr{};

    // DXCユーティリティの生成
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));

    // DXCコンパイラの生成
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));

    // デフォルトインクルードハンドラの生成
    hr = dxcUtils_->CreateDefaultIncludeHandler(&dxcIncludeHandler_);
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeImGui()
{
    // バージョンチェック & コンテキスト作成
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // スタイル設定（好きなものに変えてOK）
    ImGui::StyleColorsDark();

    // Win32 用初期化（ウィンドウハンドルが必要）
    ImGui_ImplWin32_Init(winApp_->GetHwnd());

    // DirectX12 用初期化
    // SRVヒープは shaderVisible=true で作ってあるので、そのまま渡せる
    ImGui_ImplDX12_Init(
        device_.Get(),
        kBackBufferCount,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        srvHeap_.Get(),
        srvHeap_->GetCPUDescriptorHandleForHeapStart(),
        srvHeap_->GetGPUDescriptorHandleForHeapStart());
}

Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(
    const std::wstring& filePath,
    const wchar_t* profile)
{
    assert(dxcUtils_);
    assert(dxcCompiler_);

    // 1. HLSLファイルを読み込む
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
    HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &sourceBlob);
    assert(SUCCEEDED(hr));

    // 2. DXC に渡すバッファ情報
    DxcBuffer sourceBuffer{};
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();
    sourceBuffer.Encoding = DXC_CP_UTF8;   // UTF-8想定

    // 3. コンパイル引数
    std::vector<LPCWSTR> arguments;
    arguments.push_back(filePath.c_str()); // ソース名（エラー表示用）

    arguments.push_back(L"-E");
    arguments.push_back(L"main");          // エントリポイント名

    arguments.push_back(L"-T");
    arguments.push_back(profile);          // "vs_6_0" とか "ps_6_0"

#ifdef _DEBUG
    arguments.push_back(L"-Zi");
    arguments.push_back(L"-Qembed_debug");
    arguments.push_back(L"-Od");
#else
    arguments.push_back(L"-O3");
#endif

    // 4. コンパイル実行
    Microsoft::WRL::ComPtr<IDxcResult> result;
    hr = dxcCompiler_->Compile(
        &sourceBuffer,
        arguments.data(),
        static_cast<UINT>(arguments.size()),
        dxcIncludeHandler_.Get(),
        IID_PPV_ARGS(&result));
    assert(SUCCEEDED(hr));

    // 5. エラー＆警告メッセージを取得
    Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
    hr = result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

    // コンパイル結果のステータスを確認
    HRESULT status = S_OK;
    result->GetStatus(&status);

    if (errors && errors->GetStringLength() > 0) {
        // ワーニング／エラー問わずログに出す
        Log(errors->GetStringPointer());
    }

    // コンパイル失敗のときだけ落とす
    if (FAILED(status)) {
        assert(false);
    }

    // 6. コンパイル結果（バイナリ）を取得
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
    hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    return shaderBlob;
}


Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
    assert(device_);

    // ヒープ設定（UploadHeap）
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    // リソース設定（バッファ用）
    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeInBytes;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // リソース作成
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata)
{
    assert(device_);

    // metadata を基に Resource の設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width);
    resourceDesc.Height = UINT(metadata.height);
    resourceDesc.MipLevels = UINT(metadata.mipLevels);
    resourceDesc.DepthOrArraySize = UINT(metadata.arraySize);
    resourceDesc.Format = metadata.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

    // 利用するヒープの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

    // Resource の生成
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

void DirectXCommon::UploadTextureData(
    const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
    const DirectX::ScratchImage& mipImages)
{
    assert(device_);
    assert(commandQueue_);
    assert(fence_);

    HRESULT hr{};

    // ① アップロード専用のコマンドアロケータ＆コマンドリストを作成
    ComPtr<ID3D12CommandAllocator> uploadAllocator;
    hr = device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&uploadAllocator));
    assert(SUCCEEDED(hr));

    ComPtr<ID3D12GraphicsCommandList> uploadList;
    hr = device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        uploadAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&uploadList));
    assert(SUCCEEDED(hr));

    // ② サブリソース情報を作成
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(
        device_.Get(),
        mipImages.GetImages(),
        mipImages.GetImageCount(),
        mipImages.GetMetadata(),
        subresources);

    // 中間アップロードバッファ
    UINT64 intermediateSize =
        GetRequiredIntermediateSize(
            texture.Get(), 0,
            static_cast<UINT>(subresources.size()));

    auto intermediate =
        CreateBufferResource(static_cast<size_t>(intermediateSize));

    // ③ UpdateSubresources で COPY_DEST にコピー
    UpdateSubresources(
        uploadList.Get(),
        texture.Get(),
        intermediate.Get(),
        0, 0,
        static_cast<UINT>(subresources.size()),
        subresources.data());

    // コピー完了後、テクスチャを SHADER_RESOURCE 用に遷移
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    uploadList->ResourceBarrier(1, &barrier);

    // ④ コマンドを閉じて実行
    hr = uploadList->Close();
    assert(SUCCEEDED(hr));

    ID3D12CommandList* lists[] = { uploadList.Get() };
    commandQueue_->ExecuteCommandLists(_countof(lists), lists);

    // ⑤ フェンスで GPU 完了を待つ
    fenceValue_++;
    hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
    assert(SUCCEEDED(hr));

    if (fence_->GetCompletedValue() < fenceValue_) {
        hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        assert(SUCCEEDED(hr));
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    // ここまで来れば intermediate / uploadList / uploadAllocator は
    // GPU 側の処理が終わっているので破棄されてOK（スコープアウト）
}

// すでにある UploadTextureData の下あたりに追加
DirectX::ScratchImage DirectXCommon::LoadTexture(const std::string& filePath)
{
    // std::string → std::wstring に変換
    std::wstring filePathW(filePath.begin(), filePath.end());

    DirectX::ScratchImage image{};
    DirectX::TexMetadata metadata{};

    // PNG / JPG など WIC 対応画像を読み込む
    HRESULT hr = DirectX::LoadFromWICFile(
        filePathW.c_str(),
        DirectX::WIC_FLAGS_FORCE_SRGB, // sRGB を想定
        &metadata,
        image);
    assert(SUCCEEDED(hr));

    // ミップマップ生成
    DirectX::ScratchImage mipImages{};
    hr = DirectX::GenerateMipMaps(
        image.GetImages(),
        image.GetImageCount(),
        metadata,
        DirectX::TEX_FILTER_SRGB,
        0,
        mipImages);
    assert(SUCCEEDED(hr));

    return mipImages;  // これをそのまま返して使う
}

void DirectXCommon::InitializeFixFPS() {
    // 現在時間を記録
    reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::UpdateFixFPS() {

    // 1/60秒の時間（約16666マイクロ秒）
    const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));

    // 1/60秒よりわずかに短いチェック用（65fps相当）
    const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

    // 現在時間
    auto now = std::chrono::steady_clock::now();

    // 前回からの経過時間
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

    // 1/60秒経っていない場合 → 1マイクロ秒ずつ寝る
    if (elapsed < kMinTime) {
        while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
            // 1マイクロ秒寝る
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }

    // 現在時間を次回用に記録
    reference_ = std::chrono::steady_clock::now();
}


