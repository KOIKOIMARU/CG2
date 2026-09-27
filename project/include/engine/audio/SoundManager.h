#pragma once
#include <xaudio2.h>
#include <wrl/client.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct SoundData {
    WAVEFORMATEX wfex{};
    std::vector<BYTE> buffer; // PCM波形。再生ボイスを破棄するまで保持する。
};

// 効果音と短いBGMループ用。ロード時だけボイスとPCMを確保し、戦闘中にはデコードしない。
class SoundManager {
public:
    SoundManager() = default;
    ~SoundManager();
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;
    bool Initialize(); // 音声デバイスが使えなくてもゲームは継続できる。
    void Finalize();
    bool Load(const std::string& key, const std::string& filename,
        uint32_t voiceCount = 3, float volume = 0.5f, float minimumInterval = 0.04f);
    void Unload(const std::string& key);
    bool Play(const std::string& key); // 満杯時は追加生成せず、その一音だけ見送る。
    bool PlayLoop(const std::string& key); // 1ボイスでロードした曲を無限ループ。再呼び出しで重ねない。
    void Stop(const std::string& key);
    void SetVolume(const std::string& key, float volume);
    uint32_t GetLoopCount() const;
    uint32_t GetVoiceCount() const { return voiceCount_; }
    uint64_t GetPlayCount() const { return playCount_; }
private:
    struct Entry {
        SoundData data;
        std::array<IXAudio2SourceVoice*, 4> voices{};
        float minimumInterval = 0.04f; // 同じ音の重なり過ぎを防ぐ秒数。
        bool looping = false;
        std::chrono::steady_clock::time_point lastPlay{};
    };
    SoundData LoadFile(const std::string& filename);
    void DestroyEntry(Entry& entry);
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_ = nullptr;
    std::unordered_map<std::string, Entry> sounds_;
    bool mediaFoundationStarted_ = false;
    uint32_t voiceCount_ = 0; // 効果音・BGM合計の確保数。最大32。
    uint64_t playCount_ = 0;
};
