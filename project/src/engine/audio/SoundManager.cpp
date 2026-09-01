#include "engine/audio/SoundManager.h"
#include "engine/base/StringUtility.h"
#include <algorithm>
#include <combaseapi.h>
#include <format>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace {

void ThrowIfFailed(HRESULT result, const char* operation)
{
    if (FAILED(result)) {
        throw std::runtime_error(std::format(
            "SoundManager: {} failed (HRESULT=0x{:08X})",
            operation,
            static_cast<uint32_t>(result)));
    }
}

}

SoundManager::~SoundManager()
{
    Finalize();
}

void SoundManager::Initialize() {
    if (xAudio2_) {
        return;
    }

    // 音声ファイルのデコードに使用するMedia Foundationを初期化する。
    HRESULT result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    ThrowIfFailed(result, "MFStartup");
    isMediaFoundationStarted_ = true;

    // XAudio2本体と最終出力Voiceを順に生成する。
    result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(result)) {
        Finalize();
        ThrowIfFailed(result, "XAudio2Create");
    }

    result = xAudio2_->CreateMasteringVoice(&masterVoice_);
    if (FAILED(result)) {
        Finalize();
        ThrowIfFailed(result, "CreateMasteringVoice");
    }
}

void SoundManager::Finalize() {
    // PCMバッファを解放する前に、それを参照中のVoiceをすべて破棄する。
    StopAllVoices();

    for (auto& [key, sd] : sounds_) {
        UnloadFile(sd);
    }
    sounds_.clear();

    if (masterVoice_) {
        masterVoice_->DestroyVoice();
        masterVoice_ = nullptr;
    }
    xAudio2_.Reset();

    if (isMediaFoundationStarted_) {
        MFShutdown();
        isMediaFoundationStarted_ = false;
    }
}

void SoundManager::Load(const std::string& key, const std::string& filename) {
    if (sounds_.contains(key)) {
        Unload(key);
    }
    sounds_[key] = LoadFile(filename);
}

void SoundManager::Unload(const std::string& key) {
    auto it = sounds_.find(key);
    if (it == sounds_.end()) return;
    StopVoicesForKey(key);
    UnloadFile(it->second);
    sounds_.erase(it);
}

bool SoundManager::Play(const std::string& key) {
    if (!xAudio2_) {
        return false;
    }

    CleanupFinishedVoices();

    auto it = sounds_.find(key);
    if (it == sounds_.end() || it->second.buffer.empty()) {
        return false;
    }

    const SoundData& soundData = it->second;

    IXAudio2SourceVoice* sourceVoice = nullptr;
    HRESULT result = xAudio2_->CreateSourceVoice(&sourceVoice, &soundData.wfex);
    if (FAILED(result) || !sourceVoice) {
        return false;
    }

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.buffer.data();
    buf.AudioBytes = static_cast<UINT32>(soundData.buffer.size());
    buf.Flags = XAUDIO2_END_OF_STREAM;

    result = sourceVoice->SubmitSourceBuffer(&buf);
    if (FAILED(result)) {
        sourceVoice->DestroyVoice();
        return false;
    }
    result = sourceVoice->Start();
    if (FAILED(result)) {
        sourceVoice->DestroyVoice();
        return false;
    }

    activeVoices_.push_back({ sourceVoice, key });
    return true;
}


SoundData SoundManager::LoadFile(const std::string& filename) {
    // 呼び出し側から受け取った相対パスまたは絶対パスをそのまま解決する。
    std::string fullpath = filename;

    // Media FoundationのURL引数へ渡すためUTF-16へ変換する。
    std::wstring filePathW = StringUtility::ConvertString(fullpath);

    HRESULT result{};

    // ファイル形式を問わずPCMへ変換できるSource Readerを生成する。
    ComPtr<IMFSourceReader> pReader;
    result = MFCreateSourceReaderFromURL(filePathW.c_str(), nullptr, &pReader);
    ThrowIfFailed(result, "MFCreateSourceReaderFromURL");

    // PCM形式にフォーマット指定
    ComPtr<IMFMediaType> pPCMType;
    result = MFCreateMediaType(&pPCMType);
    ThrowIfFailed(result, "MFCreateMediaType");
    result = pPCMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    ThrowIfFailed(result, "Set audio major type");
    result = pPCMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    ThrowIfFailed(result, "Set PCM subtype");

    result = pReader->SetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        nullptr,
        pPCMType.Get());
    ThrowIfFailed(result, "SetCurrentMediaType");

    // 実際にセットされたメディアタイプを取得
    ComPtr<IMFMediaType> pOutType;
    result = pReader->GetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        &pOutType);
    ThrowIfFailed(result, "GetCurrentMediaType");

    // WaveFormat取得
    WAVEFORMATEX* waveFormat = nullptr;
    result = MFCreateWaveFormatExFromMFMediaType(pOutType.Get(), &waveFormat, nullptr);
    ThrowIfFailed(result, "MFCreateWaveFormatExFromMFMediaType");

    SoundData soundData{};
    soundData.wfex = *waveFormat;

    // 用済みなので解放（MFが確保したメモリ）
    CoTaskMemFree(waveFormat);

    // PCM波形データの取得（サンプルを繋げる）
    while (true) {
        ComPtr<IMFSample> pSample;
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG llTimeStamp = 0;

        result = pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            &streamIndex,
            &flags,
            &llTimeStamp,
            &pSample);
        ThrowIfFailed(result, "ReadSample");

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }

        if (pSample) {
            ComPtr<IMFMediaBuffer> pBuffer;
            result = pSample->ConvertToContiguousBuffer(&pBuffer);
            ThrowIfFailed(result, "ConvertToContiguousBuffer");

            BYTE* pData = nullptr;
            DWORD maxLength = 0;
            DWORD currentLength = 0;

            result = pBuffer->Lock(&pData, &maxLength, &currentLength);
            ThrowIfFailed(result, "IMFMediaBuffer::Lock");

            // 複数サンプルに分割された音声を再生順に連結する。
            soundData.buffer.insert(soundData.buffer.end(), pData, pData + currentLength);

            pBuffer->Unlock();
        }
    }

    return soundData;
}

void SoundManager::UnloadFile(SoundData& soundData) {
    soundData.buffer.clear();
    soundData.wfex = {};
}

void SoundManager::CleanupFinishedVoices()
{
    const auto newEnd = std::remove_if(
        activeVoices_.begin(),
        activeVoices_.end(),
        [](ActiveVoice& activeVoice) {
            if (!activeVoice.voice) {
                return true;
            }

            XAUDIO2_VOICE_STATE state{};
            activeVoice.voice->GetState(
                &state,
                XAUDIO2_VOICE_NOSAMPLESPLAYED);
            if (state.BuffersQueued != 0) {
                return false;
            }

            activeVoice.voice->DestroyVoice();
            activeVoice.voice = nullptr;
            return true;
        });
    activeVoices_.erase(newEnd, activeVoices_.end());
}

void SoundManager::StopVoicesForKey(const std::string& key)
{
    const auto newEnd = std::remove_if(
        activeVoices_.begin(),
        activeVoices_.end(),
        [&key](ActiveVoice& activeVoice) {
            if (activeVoice.soundKey != key) {
                return false;
            }
            if (activeVoice.voice) {
                activeVoice.voice->DestroyVoice();
                activeVoice.voice = nullptr;
            }
            return true;
        });
    activeVoices_.erase(newEnd, activeVoices_.end());
}

void SoundManager::StopAllVoices()
{
    for (ActiveVoice& activeVoice : activeVoices_) {
        if (activeVoice.voice) {
            activeVoice.voice->DestroyVoice();
            activeVoice.voice = nullptr;
        }
    }
    activeVoices_.clear();
}
