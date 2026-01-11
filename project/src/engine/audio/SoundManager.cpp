#include "engine/audio/SoundManager.h"
#include <fstream>
#include <cassert>
#include <cstring>

void SoundManager::Initialize() {
    HRESULT result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(result));

    result = xAudio2_->CreateMasteringVoice(&masterVoice_);
    assert(SUCCEEDED(result));
}

void SoundManager::Finalize() {
    // 先にXAudio2止める（資料意図）
    xAudio2_.Reset();

    // その後で全部解放
    for (auto& [key, sd] : sounds_) {
        UnloadWave(sd);
    }
    sounds_.clear();
}

void SoundManager::Load(const std::string& key, const char* filename) {
    // 上書きロード対策
    if (sounds_.contains(key)) {
        Unload(key);
    }
    sounds_[key] = LoadWave(filename);
}

void SoundManager::Unload(const std::string& key) {
    auto it = sounds_.find(key);
    if (it == sounds_.end()) return;
    UnloadWave(it->second);
    sounds_.erase(it);
}

void SoundManager::Play(const std::string& key) {
    auto it = sounds_.find(key);
    assert(it != sounds_.end());

    const SoundData& soundData = it->second;

    IXAudio2SourceVoice* pSourceVoice = nullptr;
    HRESULT result = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    result = pSourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(result));
    result = pSourceVoice->Start();
    assert(SUCCEEDED(result));

    // ※授業資料レベルならDestroyVoiceは省略でもOK
    // 本気でやるなら再生終了後にDestroyVoiceする管理が必要
}

SoundData SoundManager::LoadWave(const char* filename) {
    std::ifstream file;
    file.open(filename, std::ios_base::binary);
    assert(file.is_open());

    RiffHeader riff{};
    file.read((char*)&riff, sizeof(riff));
    assert(std::strncmp(riff.chunk.id, "RIFF", 4) == 0);
    assert(std::strncmp(riff.type, "WAVE", 4) == 0);

    FormatChunk format{};
    file.read((char*)&format, sizeof(ChunkHeader));
    assert(std::strncmp(format.chunk.id, "fmt ", 4) == 0);
    assert(format.chunk.size <= sizeof(format.fmt));
    file.read((char*)&format.fmt, format.chunk.size);

    ChunkHeader data{};
    file.read((char*)&data, sizeof(data));

    if (std::strncmp(data.id, "JUNK", 4) == 0) {
        file.seekg(data.size, std::ios_base::cur);
        file.read((char*)&data, sizeof(data));
    }

    assert(std::strncmp(data.id, "data", 4) == 0);

    char* pBuffer = new char[data.size];
    file.read(pBuffer, data.size);
    file.close();

    SoundData soundData{};
    soundData.wfex = format.fmt;
    soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
    soundData.bufferSize = data.size;
    return soundData;
}

void SoundManager::UnloadWave(SoundData& soundData) {
    delete[] soundData.pBuffer;
    soundData.pBuffer = nullptr;
    soundData.bufferSize = 0;
    soundData.wfex = {};
}
