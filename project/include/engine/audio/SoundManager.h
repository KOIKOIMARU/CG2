#pragma once
#include <xaudio2.h>
#include <wrl/client.h>
#include <unordered_map>
#include <string>
#include <cstdint>

struct ChunkHeader { char id[4]; int32_t size; };
struct RiffHeader { ChunkHeader chunk; char type[4]; };
struct FormatChunk { ChunkHeader chunk; WAVEFORMATEX fmt; };

struct SoundData {
    WAVEFORMATEX wfex{};
    BYTE* pBuffer = nullptr;
    unsigned int bufferSize = 0;
};

class SoundManager {
public:
    void Initialize();
    void Finalize();

    void Load(const std::string& key, const char* filename);
    void Unload(const std::string& key);
    void Play(const std::string& key);

private:
    SoundData LoadWave(const char* filename);
    void UnloadWave(SoundData& soundData);

private:
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_ = nullptr;

    std::unordered_map<std::string, SoundData> sounds_;
};
