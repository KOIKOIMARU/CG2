#pragma once
#include <xaudio2.h>
#include <wrl/client.h>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <vector>

#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

// XAudio2へ渡せる形式情報と、デコード済みPCMデータの組。
struct SoundData {
    WAVEFORMATEX wfex{};     // チャンネル数やサンプルレートなどの再生形式
    std::vector<BYTE> buffer;// 音声ファイルからデコードしたPCMデータ
};

// 音声ファイルをキーでキャッシュし、効果音として再生する。
class SoundManager {
public:
    SoundManager() = default;
    ~SoundManager();
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;
    SoundManager(SoundManager&&) = delete;
    SoundManager& operator=(SoundManager&&) = delete;

    // XAudio2とMedia Foundationを初期化する。
    void Initialize();
    void Finalize();

    // filenameを読み込み、ゲーム側が扱いやすいkeyで登録する。
    void Load(const std::string& key, const std::string& filename);
    void Unload(const std::string& key);
    // 登録済み音声を1回再生する。未初期化またはキー不明ならfalseを返す。
    bool Play(const std::string& key);

private:
    struct ActiveVoice {
        IXAudio2SourceVoice* voice = nullptr; // 再生中のSource Voice
        std::string soundKey;                 // 参照しているsounds_のキー
    };

    // Media Foundationで対応ファイルをPCMへデコードする。
    SoundData LoadFile(const std::string& filename);
    void UnloadFile(SoundData& soundData);
    void CleanupFinishedVoices();
    void StopVoicesForKey(const std::string& key);
    void StopAllVoices();

private:
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_; // 音声再生エンジン本体
    IXAudio2MasteringVoice* masterVoice_ = nullptr; // 最終出力先。Finalizeで破棄する

    std::unordered_map<std::string, SoundData> sounds_; // キーごとの所有済みPCMデータ
    std::vector<ActiveVoice> activeVoices_; // バッファの寿命中に存在するSource Voice
    bool isMediaFoundationStarted_ = false; // MFStartupとMFShutdownの対応を管理する
};
