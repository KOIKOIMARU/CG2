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
    // XAudio2とMedia Foundationを初期化する。
    void Initialize();
    void Finalize();

    // filenameを読み込み、ゲーム側が扱いやすいkeyで登録する。
    void Load(const std::string& key, const std::string& filename);
    void Unload(const std::string& key);
    void Play(const std::string& key);

private:
    // Media Foundationで対応ファイルをPCMへデコードする。
    SoundData LoadFile(const std::string& filename);
    void UnloadFile(SoundData& soundData);

private:
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_; // 音声再生エンジン本体
    IXAudio2MasteringVoice* masterVoice_ = nullptr; // 最終出力先。Finalizeで破棄する

    std::unordered_map<std::string, SoundData> sounds_; // キーごとの所有済みPCMデータ
};
