#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <dxcapi.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "engine/base/WinApp.h"
#include "engine/base/Math.h"
#include "DirectXTex.h"
#include <chrono>
#include <thread>

class SrvManager;

// DirectX 12デバイスとフレーム描画を管理する低レベル基盤。
// 利用順は Initialize -> 毎フレーム PreDraw / 描画 / PostDraw。
// 取得できるD3D12オブジェクトは本クラス所有のため、呼び出し側で解放しないこと。
class DirectXCommon
{
public:
    // スワップチェーンと内部レンダーターゲットで使用するRTV配置。
    static constexpr UINT kBackBufferCount = 2;
    static constexpr UINT kRenderTextureRTVIndex = kBackBufferCount;
    static constexpr UINT kPostEffectTextureRTVIndex = kBackBufferCount + 1;
    static constexpr UINT kBloomHalfARTVIndex = kBackBufferCount + 2;
    static constexpr UINT kBloomHalfBRTVIndex = kBackBufferCount + 3;
    static constexpr UINT kBloomQuarterARTVIndex = kBackBufferCount + 4;
    static constexpr UINT kBloomQuarterBRTVIndex = kBackBufferCount + 5;
    static constexpr UINT kRTVDescriptorCount = kBackBufferCount + 6;

    ~DirectXCommon();
    void Initialize(WinApp* winApp);


    // 所有しているDirectX 12オブジェクトを借用して返す。
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

    ID3D12Fence* GetFence() const { return fence_.Get(); }
    UINT64& GetFenceValue() { return fenceValue_; }
    HANDLE       GetFenceEvent() const { return fenceEvent_; }

    IDxcUtils* GetDxcUtils() const { return dxcUtils_.Get(); }
    IDxcCompiler3* GetDxcCompiler() const { return dxcCompiler_.Get(); }
    IDxcIncludeHandler* GetDxcIncludeHandler() const { return dxcIncludeHandler_.Get(); }

    // CPU書込み用または指定ヒープ上の汎用バッファを生成する。
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(
        size_t sizeInBytes,
        D3D12_HEAP_TYPE heapType,
        D3D12_RESOURCE_STATES initialState,
        D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE);

    // metadataと同じ寸法・形式を持つGPUテクスチャを生成する。
    Microsoft::WRL::ComPtr<ID3D12Resource>
        CreateTextureResource(const DirectX::TexMetadata& metadata);

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(
        ID3D12Device* device,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format,
        const Math::Vector4& clearColor);

    void InitializeRenderTexture(SrvManager* srvManager);
    D3D12_GPU_DESCRIPTOR_HANDLE GetRenderTextureGpuDescriptorHandle() const;
    Math::Vector2 GetRenderTextureSize() const;

    // Batchで囲むと複数テクスチャの転送を1回のGPU待機へまとめられる。
    void UploadTextureData(
        const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
        const DirectX::ScratchImage& mipImages);
    void BeginTextureUploadBatch();
    void EndTextureUploadBatch();

    // PreDraw でコマンド記録を開始し、PostDraw でPresentとGPU同期を行う。
	void PreDraw();
    void DrawRenderTextureToSwapChain(int postEffectMode);
    void SetPostEffectProjectionMatrix(const Math::Matrix4x4& projectionMatrix);
	// 描画後処理
	void PostDraw();

    // シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
        const std::wstring& filePath,
        const wchar_t* profile);

    // 画像ファイルを読み、ミップマップ生成済みのCPU画像を返す。
    static DirectX::ScratchImage LoadTexture(
        const std::string& filePath,
        bool forceSrgb = true);

    // 指定した用途と個数のディスクリプタヒープを生成する。
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE heapType,
        UINT numDescriptors,
        bool shaderVisible);

    float GetDeltaTime() const { return deltaTime_; }

    struct FrameTiming {
        float updateMs = 0.0f;      // ゲームとエディター更新のCPU時間
        float preDrawMs = 0.0f;     // 描画開始準備のCPU時間
        float sceneDrawMs = 0.0f;   // 3D/2Dシーン描画のCPU時間
        float postEffectMs = 0.0f;  // ポストエフェクト記録のCPU時間
        float imguiDrawMs = 0.0f;   // ImGui描画記録のCPU時間
        float postDrawMs = 0.0f;    // コマンド送信と同期を含むCPU時間
        float presentMs = 0.0f;     // Present呼出しに要した時間
        float fenceWaitMs = 0.0f;   // GPU完了待ちに要した時間
        float fpsWaitMs = 0.0f;     // 上限FPSに合わせて待機した時間
        float frameCpuMs = 0.0f;    // 1フレーム全体のCPU時間
    };
    FrameTiming& EditFrameTiming() { return frameTiming_; }
    const FrameTiming& GetFrameTiming() const { return frameTiming_; }

    // スワップチェーンが交互に使用するバックバッファ数。
    size_t GetSwapChainResourcesNum() const { return kBackBufferCount; }



private:
    struct RadialBlurParameter {
        Math::Vector2 center; // 放射ブラーの中心UV座標
        float blurWidth;      // 中心から外側へ伸ばすサンプル幅
        float padding;        // 16バイト境界にそろえる余白
    };

    struct DissolveParameter {
        float threshold;       // ノイズを切り抜く進行しきい値
        float edgeWidth;       // 切り抜き境界の発光幅
        Math::Vector2 padding; // 16バイト境界にそろえる余白
    };

    struct RandomParameter {
        float time;       // ノイズ模様を変化させる累積時刻
        float padding[3]; // 16バイト境界にそろえる余白
    };

    struct GameToneParameter {
        Math::Matrix4x4 projectionInverse; // 深度からビュー空間位置を復元する逆投影行列
        float vignetteStrength;            // 画面周辺を暗くする強さ
        float saturation;                  // 色の鮮やかさ
        float contrast;                    // 明暗差
        float damageTint;                  // 被弾色の合成率
        float fogStart;                    // 霧が現れ始める距離
        float fogEnd;                      // 霧が最大になる距離
        float fogStrength;                 // 距離霧の強さ
        float horizonFogStrength;          // 地平線付近の追加霧
        float exposure;                    // 画面全体の露出
        float blackPoint;                  // 黒として締めるしきい値
        float highlightCompression;        // 高輝度の白飛びを抑える量
        float colorTemperature;            // 暖色・寒色方向の色補正
    };

    struct DepthOutlineParameter {
        Math::Matrix4x4 projectionInverse; // 深度差をビュー空間距離へ戻す逆投影行列
    };

    struct BloomParameter {
        Math::Vector2 texelSize; // 入力画像1画素分のUVサイズ
        Math::Vector2 direction; // 横または縦ブラーの方向
        float threshold;         // ブルーム抽出を始める輝度
        float intensity;         // 元画像へ戻す発光強度
        float scatter;           // 発光の広がり
        float padding;           // 16バイト境界にそろえる余白
    };

    struct BloomRenderTexture {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource; // 縮小・ぼかし途中の描画先
        uint32_t srvIndex = 0;                            // シェーダー入力用SRV番号
        UINT rtvIndex = 0;                                // 描画先RTV番号
        uint32_t width = 0;                               // テクスチャ横幅
        uint32_t height = 0;                              // テクスチャ縦幅
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_RENDER_TARGET; // 現在の状態
    };

    // Initializeから順番に呼ぶ部分初期化と診断処理。
    void InitializeDiagnosticLog();
    void ConfigureDred();
    void FlushDebugMessages();
    void AppendDiagnosticLog(std::string_view message) const;
    void CheckDeviceOperation(
        HRESULT result,
        std::string_view operation);
    void ReportDeviceRemovedDiagnostics(
        std::string_view operation,
        HRESULT failureResult);
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
        const std::wstring& pixelShaderPath,
        DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    void TransitionResource(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES& currentState,
        D3D12_RESOURCE_STATES nextState);
    void DrawFullscreenPass(
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
        uint32_t width,
        uint32_t height,
        ID3D12PipelineState* pipelineState,
        uint32_t sourceSrvIndex,
        uint32_t secondarySrvIndex,
        D3D12_GPU_VIRTUAL_ADDRESS parameterAddress);
    void DrawBloomPasses(
        uint32_t sourceSrvIndex,
        uint32_t sourceWidth,
        uint32_t sourceHeight,
        float threshold,
        float scatter);
    void DrawBloomCompositeToBackBuffer(
        uint32_t sourceSrvIndex,
        float intensity);

    // ヒープ先頭と番号からCPU/GPUディスクリプタハンドルを計算する。
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize, uint32_t index);
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize, uint32_t index);

    WinApp* winApp_ = nullptr; // 描画先ウィンドウの借用先

    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_; // GPUとスワップチェーンの生成元
    Microsoft::WRL::ComPtr<ID3D12Device> device_;       // DirectX 12デバイス本体
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue_; // Debug Layerメッセージの取得先
    std::wstring diagnosticLogPath_;                    // D3D12診断ログの出力先
    bool deviceRemovedDiagnosticsReported_ = false;     // デバイスロスト詳細の重複出力防止

    // コマンド関連
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;         // GPUへコマンドを送るキュー
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_; // 毎フレームのコマンド記録領域
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;   // 毎フレームの描画コマンド列
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> textureUploadBatchAllocator_; // 一括転送専用記録領域
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> textureUploadBatchList_;   // 一括転送専用コマンド列
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>
        textureUploadBatchIntermediates_; // GPU転送完了まで保持する中間バッファ
    bool isTextureUploadBatchActive_ = false; // テクスチャ一括転送を記録中か

    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_; // 画面表示用バックバッファの交換器

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kBackBufferCount> swapChainResources_; // 表示用画像

    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;    // 3D描画の深度画像
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTextureResource_;   // シーンを最初に描く中間画像
    Microsoft::WRL::ComPtr<ID3D12Resource> postEffectTextureResource_;// 多段エフェクトの中間画像

    // 各種デスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_; // 全描画先のRTVヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_; // メイン深度用DSVヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_; // 旧互換用のSRVヒープ参照
    uint32_t renderTextureSrvIndex_ = 0;     // シーン中間画像のSRV番号
    uint32_t postEffectTextureSrvIndex_ = 0; // エフェクト中間画像のSRV番号
    uint32_t depthTextureSrvIndex_ = 0;      // 深度画像のSRV番号
    uint32_t dissolveMaskTextureSrvIndex_ = 0; // ディゾルブ用ノイズ画像のSRV番号
    D3D12_RESOURCE_STATES renderTextureState_ =
        D3D12_RESOURCE_STATE_RENDER_TARGET; // シーン中間画像の現在状態
    D3D12_RESOURCE_STATES postEffectTextureState_ =
        D3D12_RESOURCE_STATE_RENDER_TARGET; // エフェクト中間画像の現在状態

    UINT rtvDescriptorSize_ = 0; // RTVヒープ内の1要素分のバイト間隔
    UINT dsvDescriptorSize_ = 0; // DSVヒープ内の1要素分のバイト間隔

    Microsoft::WRL::ComPtr<ID3D12Fence> fence_; // CPUとGPUの完了同期
    UINT64 fenceValue_ = 0;                     // 次に通知・待機するフェンス値
    HANDLE fenceEvent_ = nullptr;                // フェンス完了待ちのWin32イベント

    D3D12_VIEWPORT viewport_{}; // メイン描画領域の座標と深度範囲
    D3D12_RECT scissorRect_{};  // メイン描画を許可する画素範囲

    // DXC関連
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;                 // シェーダー読込と診断補助
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;          // HLSLコンパイラ
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxcIncludeHandler_;// HLSLの#include解決
    std::unordered_map<
        std::wstring,
        Microsoft::WRL::ComPtr<IDxcBlob>> shaderCache_; // パスとプロファイル単位のコンパイル結果

    // 全画面ポストエフェクトは共通ルートシグネチャと、効果別PSOを使う。
    Microsoft::WRL::ComPtr<ID3D12RootSignature> fullscreenRootSignature_; // 全画面パスの共通ルート引数
    Microsoft::WRL::ComPtr<ID3D12PipelineState> copyImagePipelineState_;  // 無加工コピー
    Microsoft::WRL::ComPtr<ID3D12PipelineState> grayscalePipelineState_;  // グレースケール
    Microsoft::WRL::ComPtr<ID3D12PipelineState> vignettePipelineState_;   // ビネット
    Microsoft::WRL::ComPtr<ID3D12PipelineState> boxFilterPipelineState_;  // 3x3ボックスフィルター
    Microsoft::WRL::ComPtr<ID3D12PipelineState> boxFilter5x5PipelineState_;// 5x5ボックスフィルター
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianFilterPipelineState_; // ガウシアンフィルター
    Microsoft::WRL::ComPtr<ID3D12PipelineState> vignetteSmoothingPipelineState_; // 平滑化付きビネット
    Microsoft::WRL::ComPtr<ID3D12PipelineState> luminanceOutlinePipelineState_; // 輝度差アウトライン
    Microsoft::WRL::ComPtr<ID3D12PipelineState> depthOutlinePipelineState_; // 深度差アウトライン
    Microsoft::WRL::ComPtr<ID3D12PipelineState> radialBlurPipelineState_; // 放射ブラー
    Microsoft::WRL::ComPtr<ID3D12PipelineState> dissolvePipelineState_;  // ディゾルブ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> randomPipelineState_;    // ランダムノイズ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gameTonePipelineState_;  // 色調・霧の統合補正
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomPipelineState_;     // 単段ブルーム互換処理
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomExtractPipelineState_; // 高輝度抽出
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomBlurPipelineState_;    // ブルームぼかし
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomDownsamplePipelineState_; // 縮小合成
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomUpsamplePipelineState_;   // 拡大合成
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomCompositePipelineState_;  // 元画像との最終合成

    Microsoft::WRL::ComPtr<ID3D12Resource> radialBlurParameterResource_; // 放射ブラー定数バッファ
    RadialBlurParameter* radialBlurParameterData_ = nullptr;              // 上記バッファのCPU書込み先
    Microsoft::WRL::ComPtr<ID3D12Resource> dissolveParameterResource_;   // ディゾルブ定数バッファ
    DissolveParameter* dissolveParameterData_ = nullptr;                  // 上記バッファのCPU書込み先
    Microsoft::WRL::ComPtr<ID3D12Resource> randomParameterResource_;     // ノイズ定数バッファ
    RandomParameter* randomParameterData_ = nullptr;                      // 上記バッファのCPU書込み先
    Microsoft::WRL::ComPtr<ID3D12Resource> gameToneParameterResource_;   // 統合色調補正の定数バッファ
    GameToneParameter* gameToneParameterData_ = nullptr;                  // 上記バッファのCPU書込み先
    Microsoft::WRL::ComPtr<ID3D12Resource> depthOutlineParameterResource_;// 深度線画の定数バッファ
    DepthOutlineParameter* depthOutlineParameterData_ = nullptr;          // 上記バッファのCPU書込み先
    Microsoft::WRL::ComPtr<ID3D12Resource> bloomParameterResource_;      // ブルーム処理の定数バッファ
    BloomParameter* bloomParameterData_ = nullptr;                        // 上記バッファのCPU書込み先
    std::array<BloomRenderTexture, 4> bloomTextures_;                     // 半分・4分の1解像度の往復描画先
    Microsoft::WRL::ComPtr<ID3D12Resource> dissolveMaskTextureResource_; // ディゾルブ用ノイズ画像
    float dissolveThreshold_ = 0.0f;                                     // ディゾルブの現在進行度
    float randomTime_ = 1.0f;                                            // ノイズ変化用の累積時刻

    // 上限FPSに合わせるための時刻初期化とフレーム末尾の待機。
    void InitializeFixFPS();
    void UpdateFixFPS();

    std::chrono::steady_clock::time_point reference_; // 前フレーム開始時の基準時刻
    UINT currentBackBufferIndex_ = 0;                  // 今フレームに表示するバックバッファ番号
    float deltaTime_ = 1.0f / 60.0f;                   // 前フレームからの経過秒数
    FrameTiming frameTiming_{};                        // 計測したCPU処理時間の最新値
};
