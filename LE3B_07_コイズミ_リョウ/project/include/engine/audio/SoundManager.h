#pragma once
#include <xaudio2.h>
#include <wrl/client.h>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <vector>

#include <mfapi.h>
#include <mfidl.h>       // ★これが重要（IMFMediaSource/IMFAttributes等）
#include <mfobjects.h>   // ★これも入れとくと安定
#include <mfreadwrite.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

struct SoundData {
    WAVEFORMATEX wfex{};
    std::vector<BYTE> buffer;   // ★ここが変更点
};

class SoundManager {
public:
    void Initialize();
    void Finalize();

    void Load(const std::string& key, const std::string& filename); // ★char* → string推奨
    void Unload(const std::string& key);
    void Play(const std::string& key);

private:
    SoundData LoadFile(const std::string& filename); // ★LoadWave → LoadFile
    void UnloadFile(SoundData& soundData);

private:
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_ = nullptr;

    std::unordered_map<std::string, SoundData> sounds_;
};
