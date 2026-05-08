#include "engine/base/DirectXCommon.h"
#include "engine/base/Logger.h"
#include "engine/base/SrvManager.h"
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
#include <filesystem>

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

    // ★ FPS 固定の初期化
    InitializeFixFPS();
}


void DirectXCommon::PreDraw() {
    assert(renderTextureResource_);

    // 0. コマンドリストを描画用に準備
    HRESULT hr = S_OK;
    hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));

    // バックバッファの番号取得
    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
    UINT bbIndex = currentBackBufferIndex_;

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
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCPUDescriptorHandle(
        rtvHeap_,
        rtvDescriptorSize_,
        kRenderTextureRTVIndex
    );

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
        dsvHeap_->GetCPUDescriptorHandleForHeapStart();

    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // 4. クリア
    float clearColor[] = { 1.0f, 0.0f, 0.0f, 1.0f };
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
}

void DirectXCommon::DrawRenderTextureToSwapChain()
{
    assert(renderTextureResource_);
    assert(copyImageRootSignature_);
    assert(copyImagePipelineState_);
    assert(srvDescriptorHeap_);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = renderTextureResource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCPUDescriptorHandle(
        rtvHeap_,
        rtvDescriptorSize_,
        currentBackBufferIndex_
    );
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);

    ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.Get() };
    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList_->SetGraphicsRootSignature(copyImageRootSignature_.Get());
    commandList_->SetPipelineState(copyImagePipelineState_.Get());
    commandList_->SetGraphicsRootDescriptorTable(
        0,
        GetGPUDescriptorHandle(
            srvDescriptorHeap_,
            device_->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            ),
            renderTextureSrvIndex_
        )
    );
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->DrawInstanced(3, 1, 0, 0);

    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrier);
}


void DirectXCommon::PostDraw()
{
    HRESULT hr = S_OK;

    // バックバッファの番号取得
    UINT bbIndex = currentBackBufferIndex_; // ← 絶対に GetCurrentBackBufferIndex() を呼ばない


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
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
    rtvHeap_ = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        kRTVDescriptorCount,
        false
    );
    dsvHeap_ = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        1,
        false
    );

    // ★★★ これが無かった ★★★
    rtvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV
        );

    dsvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV
        );
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

        // SRGB の RTV 設定
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;          // ★ SRGB でビューを作る
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        rtvDesc.Texture2D.PlaneSlice = 0;

        // 対応するRTVハンドルを計算
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
            GetCPUDescriptorHandle(rtvHeap_, rtvDescriptorSize_, i);

        // RTVを生成
        device_->CreateRenderTargetView(
            swapChainResources_[i].Get(),
            &rtvDesc,                 // ★ nullptr ではなく &rtvDesc
            rtvHandle);
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


Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(
    const std::wstring& filePath,
    const wchar_t* profile)
{
    HRESULT hr = S_OK;

    // ============================
    // 1) ファイルパス → 絶対パス
    // ============================
    std::filesystem::path absPath = std::filesystem::absolute(filePath);

    if (!std::filesystem::exists(absPath)) {
        std::string msg =
            "❌ Shader file not found:\n" +
            absPath.string() +
            "\nCurrentDir: " +
            std::filesystem::current_path().string() +
            "\n";
        OutputDebugStringA(msg.c_str());
        assert(false);
        return nullptr;
    }

    // ============================
    // 2) ファイル読み込み
    // ============================
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
    hr = dxcUtils_->LoadFile(absPath.c_str(), nullptr, &shaderSource);
    if (FAILED(hr)) {
        OutputDebugStringA("❌ LoadFile failed\n");
        assert(false);
        return nullptr;
    }

    DxcBuffer shaderBuffer{};
    shaderBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderBuffer.Size = shaderSource->GetBufferSize();
    shaderBuffer.Encoding = DXC_CP_UTF8;

    // ============================
    // 3) コンパイル引数の準備
    // ============================
    LPCWSTR args[] = {
        absPath.c_str(),      // ファイル名
        L"-E", L"main",       // エントリーポイント
        L"-T", profile,       // ターゲット
        L"-Zi",               // デバッグ情報
        L"-Qembed_debug"      // デバッグ情報埋め込み
    };

    Microsoft::WRL::ComPtr<IDxcResult> result = nullptr;

    hr = dxcCompiler_->Compile(
        &shaderBuffer,
        args,
        _countof(args),
        dxcIncludeHandler_.Get(),   // ← ここが超重要（修正済）
        IID_PPV_ARGS(&result)
    );

    if (FAILED(hr)) {
        OutputDebugStringA("❌ Shader compile request failed.\n");
        assert(false);
        return nullptr;
    }

    // ============================
    // 4) エラーメッセージ取り出し
    // ============================
    Microsoft::WRL::ComPtr<IDxcBlobUtf8> errorBlob = nullptr;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorBlob), nullptr);

    if (errorBlob && errorBlob->GetStringLength() != 0) {
        OutputDebugStringA("❌ Shader Compile Error:\n");
        OutputDebugStringA(errorBlob->GetStringPointer());
        assert(false);
        return nullptr;
    }

    // ============================
    // 5) 成功したバイナリの取得
    // ============================
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);

    if (!shaderBlob) {
        OutputDebugStringA("❌ DXC returned no shader blob.\n");
        assert(false);
        return nullptr;
    }

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
DirectXCommon::CreateBufferResource(
    size_t sizeInBytes,
    D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_STATES initialState,
    D3D12_RESOURCE_FLAGS resourceFlags)
{
    assert(device_);

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = heapType;

    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeInBytes;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resDesc.Flags = resourceFlags;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        initialState,
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
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;


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

Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateRenderTextureResource(
    ID3D12Device* device,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const Math::Vector4& clearColor)
{
    assert(device);

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = format;
    clearValue.Color[0] = clearColor.x;
    clearValue.Color[1] = clearColor.y;
    clearValue.Color[2] = clearColor.z;
    clearValue.Color[3] = clearColor.w;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&resource)
    );
    assert(SUCCEEDED(hr));

    return resource;
}

void DirectXCommon::InitializeRenderTexture(SrvManager* srvManager)
{
    assert(srvManager);
    assert(device_);
    assert(rtvHeap_);

    constexpr DXGI_FORMAT kRenderTextureFormat =
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    const Math::Vector4 kClearColor{ 1.0f, 0.0f, 0.0f, 1.0f };

    renderTextureResource_ = CreateRenderTextureResource(
        device_.Get(),
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        kRenderTextureFormat,
        kClearColor
    );

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = kRenderTextureFormat;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCPUDescriptorHandle(
        rtvHeap_,
        rtvDescriptorSize_,
        kRenderTextureRTVIndex
    );
    device_->CreateRenderTargetView(
        renderTextureResource_.Get(),
        &rtvDesc,
        rtvHandle
    );

    renderTextureSrvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVforTexture2D(
        renderTextureSrvIndex_,
        renderTextureResource_.Get(),
        kRenderTextureFormat,
        1
    );
    srvDescriptorHeap_ = srvManager->GetDescriptorHeapComPtr();

    CreateCopyImageRootSignature();
    CreateCopyImagePipelineState();
}

void DirectXCommon::CreateCopyImageRootSignature()
{
    D3D12_DESCRIPTOR_RANGE descriptorRange{};
    descriptorRange.BaseShaderRegister = 0;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameter{};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameter.DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParameter.DescriptorTable.NumDescriptorRanges = 1;

    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    descriptionRootSignature.pParameters = &rootParameter;
    descriptionRootSignature.NumParameters = 1;
    descriptionRootSignature.pStaticSamplers = &staticSampler;
    descriptionRootSignature.NumStaticSamplers = 1;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob
    );
    if (FAILED(hr)) {
        if (errorBlob) {
            Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = device_->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&copyImageRootSignature_)
    );
    assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateCopyImagePipelineState()
{
    auto vertexShaderBlob =
        CompileShader(L"shaders/CopyImage.VS.hlsl", L"vs_6_0");
    auto pixelShaderBlob =
        CompileShader(L"shaders/CopyImage.PS.hlsl", L"ps_6_0");

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = false;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
    pipelineDesc.pRootSignature = copyImageRootSignature_.Get();
    pipelineDesc.InputLayout.pInputElementDescs = nullptr;
    pipelineDesc.InputLayout.NumElements = 0;
    pipelineDesc.VS = {
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize()
    };
    pipelineDesc.PS = {
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize()
    };
    pipelineDesc.BlendState = blendDesc;
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineDesc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.SampleDesc.Count = 1;
    pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = device_->CreateGraphicsPipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(&copyImagePipelineState_)
    );
    assert(SUCCEEDED(hr));
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

    HRESULT hr = S_OK;
    const std::wstring extension =
        std::filesystem::path(filePathW).extension().wstring();

    if (extension == L".dds" || extension == L".DDS") {
        hr = DirectX::LoadFromDDSFile(
            filePathW.c_str(),
            DirectX::DDS_FLAGS_NONE,
            &metadata,
            image);
        assert(SUCCEEDED(hr));
        return image;
    }

    // PNG / JPG など WIC 対応画像を読み込む
    hr = DirectX::LoadFromWICFile(
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

    const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));
    const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

    // ★ ここが追加（秒に変換）
    deltaTime_ = elapsed.count() / 1000000.0f;

    if (elapsed < kMinTime) {
        while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }

        // ★ FPS固定時は deltaTime を 1/60 に揃える
        deltaTime_ = 1.0f / 60.0f;
    }

    reference_ = std::chrono::steady_clock::now();
}
