#include "engine/audio/SoundManager.h"
#include "engine/base/Logger.h"
#include "engine/base/StringUtility.h"
#include <algorithm>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

using Microsoft::WRL::ComPtr;
SoundManager::~SoundManager() { Finalize(); }

bool SoundManager::Initialize()
{
    Finalize();
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET))) {
        Logger::Log("Audio disabled: Media Foundation initialization failed.\n");
        return false;
    }
    mediaFoundationStarted_ = true;
    if (FAILED(XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR)) ||
        FAILED(xAudio2_->CreateMasteringVoice(&masterVoice_))) {
        Logger::Log("Audio disabled: output device unavailable.\n");
        Finalize();
        return false;
    }
    masterVoice_->SetVolume(0.60f);
    playCount_ = 0;
    return true;
}

void SoundManager::DestroyEntry(Entry& entry)
{
    // DestroyVoice完了後はこの波形がオーディオスレッドから参照されない。
    for (auto*& voice : entry.voices) {
        if (voice) {
            voice->DestroyVoice();
            voice = nullptr;
            --voiceCount_;
        }
    }
    entry.data.buffer.clear();
}

void SoundManager::Finalize()
{
    for (auto& item : sounds_) {
        DestroyEntry(item.second);
    }
    sounds_.clear();
    if (masterVoice_) {
        masterVoice_->DestroyVoice();
        masterVoice_ = nullptr;
    }
    xAudio2_.Reset();
    if (mediaFoundationStarted_) {
        MFShutdown();
        mediaFoundationStarted_ = false;
    }
}

bool SoundManager::Load(const std::string& key, const std::string& filename,
    uint32_t voiceCount, float volume, float minimumInterval)
{
    if (!xAudio2_ || !masterVoice_) {
        return false;
    }
    Unload(key);
    voiceCount = std::clamp(voiceCount, 1u, 4u);
    if (voiceCount_ + voiceCount > 32u) {
        return false;
    }
    Entry entry;
    entry.data = LoadFile(filename);
    if (entry.data.buffer.empty()) {
        Logger::Log("Audio load failed: " + filename + "\n");
        return false;
    }
    entry.minimumInterval = (std::max)(minimumInterval, 0.0f);
    for (uint32_t index = 0; index < voiceCount; ++index) {
        auto*& voice = entry.voices[index];
        if (FAILED(xAudio2_->CreateSourceVoice(&voice, &entry.data.wfex))) {
            DestroyEntry(entry);
            Logger::Log("Audio voice allocation failed: " + key + "\n");
            return false;
        }
        ++voiceCount_;
        voice->SetVolume(std::clamp(volume, 0.0f, 1.0f));
    }
    sounds_.emplace(key, std::move(entry));
    return true;
}

void SoundManager::Unload(const std::string& key)
{
    const auto it = sounds_.find(key);
    if (it != sounds_.end()) {
        DestroyEntry(it->second);
        sounds_.erase(it);
    }
}

bool SoundManager::Play(const std::string& key)
{
    const auto it = sounds_.find(key);
    if (it == sounds_.end()) {
        return false;
    }
    Entry& entry = it->second;
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - entry.lastPlay).count() < entry.minimumInterval) {
        return false;
    }
    for (auto* voice : entry.voices) {
        if (!voice) {
            continue;
        }
        XAUDIO2_VOICE_STATE state{};
        voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued != 0) {
            continue;
        }
        XAUDIO2_BUFFER buffer{};
        buffer.pAudioData = entry.data.buffer.data();
        buffer.AudioBytes = static_cast<UINT32>(entry.data.buffer.size());
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start())) {
            voice->Stop();
            voice->FlushSourceBuffers();
            return false;
        }
        entry.lastPlay = now;
        ++playCount_;
        return true;
    }
    return false;
}

SoundData SoundManager::LoadFile(const std::string& filename)
{
    ComPtr<IMFSourceReader> reader;
    const std::wstring path = StringUtility::ConvertString(filename);
    if (FAILED(MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader))) {
        return {};
    }
    ComPtr<IMFMediaType> type;
    if (FAILED(MFCreateMediaType(&type)) ||
        FAILED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
        FAILED(type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
        FAILED(type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16)) ||
        FAILED(type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2)) ||
        FAILED(type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000)) ||
        FAILED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, type.Get()))) {
        return {};
    }
    ComPtr<IMFMediaType> outputType;
    if (FAILED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &outputType))) {
        return {};
    }
    WAVEFORMATEX* format = nullptr;
    if (FAILED(MFCreateWaveFormatExFromMFMediaType(outputType.Get(), &format, nullptr))) {
        return {};
    }
    SoundData sound;
    // 拡張情報を切り落とさないため、対応する通常PCM形式だけを受け付ける。
    const bool supported = format->wFormatTag == WAVE_FORMAT_PCM && format->cbSize == 0;
    if (supported) {
        sound.wfex = *format;
    }
    CoTaskMemFree(format);
    if (!supported) {
        return {};
    }
    while (true) {
        ComPtr<IMFSample> sample;
        DWORD flags = 0;
        if (FAILED(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0,
                nullptr, &flags, nullptr, &sample)) || (flags & MF_SOURCE_READERF_ERROR)) {
            return {};
        }
        if (sample) {
            ComPtr<IMFMediaBuffer> buffer;
            if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) {
                return {};
            }
            BYTE* data = nullptr;
            DWORD length = 0;
            if (FAILED(buffer->Lock(&data, nullptr, &length))) {
                return {};
            }
            // 長いBGMを誤って全展開しない。効果音は最大32MBまで。
            const bool fits = sound.buffer.size() + length <= 32u * 1024u * 1024u;
            if (fits && length > 0) {
                sound.buffer.insert(sound.buffer.end(), data, data + length);
            }
            buffer->Unlock();
            if (!fits) {
                return {};
            }
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }
    }
    return sound;
}
