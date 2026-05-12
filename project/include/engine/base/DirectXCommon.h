#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <dxcapi.h>
#include <string>
#include "engine/base/WinApp.h"
#include "engine/base/Math.h"
#include "DirectXTex.h"
#include <chrono>   // ← 必須
#include <thread>   // ← sleep_for に必要

// DirectX基盤
class SrvManager;

class DirectXCommon
{
public:
    // ★ バックバッファ数
    static constexpr UINT kBackBufferCount = 2;
    static constexpr UINT kRenderTextureRTVIndex = kBackBufferCount;
    static constexpr UINT kRTVDescriptorCount = kBackBufferCount + 1;

    // 初期化（全部まとめ）
    void Initialize(WinApp* winApp);


    // ゲッター
    ID3D12Device* GetDevice() const { return device_.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
    ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator_.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
    IDXGISwapChain4* GetSwapChain() const { return swapChain_.Get(); }

    ID3D12DescriptorHeap* GetRTVHeap() const { return rtvHeap_.Get(); }
    ID3D12DescriptorHeap* GetDSVHeap() const { return dsvHeap_.Get(); }

    UINT GetRTVDescriptorSize() const { return rtvDescriptorSize_; }
    UINT GetDSVDescriptorSize() const { return dsvDescriptorSize_; }

    const D3D12_VIEWPORT& GetViewport() const { return viewport_; }
    const D3D12_RECT& GetScissorRect() const { return scissorRect_; }

    ID3D12Fence* GetFence() const { return fence_.Get(); }          // これも const でOK
    UINT64& GetFenceValue() { return fenceValue_; }            // ここだけ非constで良い
    HANDLE       GetFenceEvent() const { return fenceEvent_; }

    IDxcUtils* GetDxcUtils() const { return dxcUtils_.Get(); }
    IDxcCompiler3* GetDxcCompiler() const { return dxcCompiler_.Get(); }
    IDxcIncludeHandler* GetDxcIncludeHandler() const { return dxcIncludeHandler_.Get(); }

    // バッファリソース生成
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(
        size_t sizeInBytes,
        D3D12_HEAP_TYPE heapType,
        D3D12_RESOURCE_STATES initialState,
        D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE);

    // テクスチャリソース生成
    Microsoft::WRL::ComPtr<ID3D12Resource>
        CreateTextureResource(const DirectX::TexMetadata& metadata);

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(
        ID3D12Device* device,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format,
        const Math::Vector4& clearColor);

    void InitializeRenderTexture(SrvManager* srvManager);

    // テクスチャデータ転送
    void UploadTextureData(
        const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
        const DirectX::ScratchImage& mipImages);

    // 描画前処理
	void PreDraw();
    void DrawRenderTextureToSwapChain(int postEffectMode);
	// 描画後処理
	void PostDraw();

    // シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
        const std::wstring& filePath,
        const wchar_t* profile);

	// テクスチャ読み込み（static／外から使う便利版）
    static DirectX::ScratchImage LoadTexture(const std::string& filePath);

    // デスクリプタヒープ生成関数（内部用）
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE heapType,
        UINT numDescriptors,
        bool shaderVisible);

    float GetDeltaTime() const { return deltaTime_; }

    // ★追加：スワップチェーンリソース数（バックバッファ数）
    size_t GetSwapChainResourcesNum() const { return kBackBufferCount; }



private:
    struct RadialBlurParameter {
        Math::Vector2 center;
        float blurWidth;
        float padding;
    };

    // --- ここから「Initialize」専用の内部関数たち ---

    // 部分初期化（外から呼ばせない）
    void InitializeDevice();
    void InitializeCommand();
    void InitializeSwapChain();
    void InitializeDepthBuffer();
    void InitializeDescriptorHeaps();
    void InitializeRenderTargetView();
    void InitializeDepthStencilView();
    void InitializeFence();
    void InitializeViewport();
    void InitializeScissorRect();
    void InitializeDXC();
    void CreateFullscreenRootSignature();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateFullscreenPipelineState(
        const std::wstring& pixelShaderPath);

    // 汎用ハンドル取得関数（static／内部用）
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize, uint32_t index);
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize, uint32_t index);

    // --- メンバ変数 ---

    // WindowsAPI（ウィンドウハンドル用）
    WinApp* winApp_ = nullptr;

    // DXGIファクトリー
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;

    // DirectX12デバイス
    Microsoft::WRL::ComPtr<ID3D12Device> device_;

    // コマンド関連
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>        commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    // スワップチェーン
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;

    // スワップチェーンリソース（バックバッファ）
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kBackBufferCount> swapChainResources_;

    // 深度バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTextureResource_;

    // 各種デスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    uint32_t renderTextureSrvIndex_ = 0;
    uint32_t depthTextureSrvIndex_ = 0;

    // 各ヒープのインクリメントサイズ
    UINT rtvDescriptorSize_ = 0;
    UINT dsvDescriptorSize_ = 0;

    // 同期（フェンス）
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    UINT64 fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;

    // ビューポートとシザー矩形
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT     scissorRect_{};

    // DXC関連
    Microsoft::WRL::ComPtr<IDxcUtils>          dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3>      dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxcIncludeHandler_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> fullscreenRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> copyImagePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> grayscalePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> vignettePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> boxFilterPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> boxFilter5x5PipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianFilterPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> luminanceOutlinePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> depthOutlinePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> radialBlurPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> radialBlurParameterResource_;
    RadialBlurParameter* radialBlurParameterData_ = nullptr;

    // ==== FPS 固定用 ====
  // FPS 固定の初期化
    void InitializeFixFPS();
    // FPS 固定の更新
    void UpdateFixFPS();

    // 前フレームの基準時間
    std::chrono::steady_clock::time_point reference_;

    UINT currentBackBufferIndex_ = 0;

    float deltaTime_ = 1.0f / 60.0f;
};
