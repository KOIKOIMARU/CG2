#pragma once
class TextureManager
{
private:
	// 初期化
	void Initialize();

	// テクスチャ1毎分のデータ
	struct TextureData {
		std::string filePath;                      // ファイルパス
		DirectX::TexMetadata metadata;          // テクスチャメタデータ
		Microsoft::WRL::ComPtr<ID3D12Resource> resource; // テクスチャリソース
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU; // SRVのCPUハンドル
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU; // SRVのGPUハンドル
	};
	// テクスチャデータ
	std::vector<TextureData> texturesDatas;

	static TextureManager* instance_;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;
public:
	// シングルトンインスタンス取得
	static TextureManager* GetInstance();
	// 終了
	void Finalize();
};

