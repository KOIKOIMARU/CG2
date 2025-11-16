#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <dxcapi.h>
#include <string>
#include "engine/base/WinApp.h"
#include "DirectXTex.h"

// DirectX基盤
class DirectXCommon
{
public:
    // ★ バックバッファ数
    static constexpr UINT kBackBufferCount = 2;

    // 初期化（全部まとめ）
    void Initialize(WinApp* winApp);

    // SRV専用の公開関数（外から使う便利版）
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

    // ゲッター
    ID3D12Device* GetDevice() const { return device_.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
    ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator_.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
    IDXGISwapChain4* GetSwapChain() const { return swapChain_.Get(); }

    ID3D12DescriptorHeap* GetRTVHeap() const { return rtvHeap_.Get(); }
    ID3D12DescriptorHeap* GetSRVHeap() const { return srvHeap_.Get(); }
    ID3D12DescriptorHeap* GetDSVHeap() const { return dsvHeap_.Get(); }

    UINT GetRTVDescriptorSize() const { return rtvDescriptorSize_; }
    UINT GetSRVDescriptorSize() const { return srvDescriptorSize_; }
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

    // テクスチャリソース生成
    Microsoft::WRL::ComPtr<ID3D12Resource>
        CreateTextureResource(const DirectX::TexMetadata& metadata);

    // テクスチャデータ転送
    void UploadTextureData(
        const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
        const DirectX::ScratchImage& mipImages);

    // 描画前処理
	void PreDraw();
	// 描画後処理
	void PostDraw();

    // シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
        const std::wstring& filePath,
        const wchar_t* profile);

	// テクスチャ読み込み（static／外から使う便利版）
    static DirectX::ScratchImage LoadTexture(const std::string& filePath);

private:
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
    void InitializeImGui();

    // デスクリプタヒープ生成関数（内部用）
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE heapType,
        UINT numDescriptors,
        bool shaderVisible);

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

    // 各種デスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;

    // 各ヒープのインクリメントサイズ
    UINT rtvDescriptorSize_ = 0;
    UINT srvDescriptorSize_ = 0;
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
};
