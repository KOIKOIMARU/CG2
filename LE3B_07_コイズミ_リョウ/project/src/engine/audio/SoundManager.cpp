#include "engine/audio/SoundManager.h"
#include "engine/base/StringUtility.h"   // ConvertString(wstring<->string)がある前提
#include <cassert>
#include <combaseapi.h>

using Microsoft::WRL::ComPtr;

void SoundManager::Initialize() {
    // ★ Media Foundation 初期化（ローカルファイル版）
    HRESULT result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    assert(SUCCEEDED(result));

    // XAudio2 初期化
    result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(result));

    result = xAudio2_->CreateMasteringVoice(&masterVoice_);
    assert(SUCCEEDED(result));
}

void SoundManager::Finalize() {
    // 先にXAudio2止める（資料意図）
    xAudio2_.Reset();

    // 音声バッファ解放
    for (auto& [key, sd] : sounds_) {
        UnloadFile(sd);
    }
    sounds_.clear();

    // ★ Media Foundation 終了
    HRESULT result = MFShutdown();
    assert(SUCCEEDED(result));
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
    UnloadFile(it->second);
    sounds_.erase(it);
}

void SoundManager::Play(const std::string& key) {
    auto it = sounds_.find(key);
    assert(it != sounds_.end());

    const SoundData& soundData = it->second;
    assert(!soundData.buffer.empty());

    IXAudio2SourceVoice* pSourceVoice = nullptr;
    HRESULT result = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.buffer.data();                 // ★ここ
    buf.AudioBytes = static_cast<UINT32>(soundData.buffer.size()); // ★ここ
    buf.Flags = XAUDIO2_END_OF_STREAM;

    result = pSourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(result));
    result = pSourceVoice->Start();
    assert(SUCCEEDED(result));
}


SoundData SoundManager::LoadFile(const std::string& filename) {
    // フルパス化（必要なら）
    // すでに resources/xxx.mp3 みたいに渡すならそのままでもOK
    std::string fullpath = filename;

    // ワイド文字列に変換（MFはURL引数がwchar前提）
    std::wstring filePathW = StringUtility::ConvertString(fullpath);

    HRESULT result{};

    // SourceReader 作成
    ComPtr<IMFSourceReader> pReader;
    result = MFCreateSourceReaderFromURL(filePathW.c_str(), nullptr, &pReader);
    assert(SUCCEEDED(result));

    // PCM形式にフォーマット指定
    ComPtr<IMFMediaType> pPCMType;
    result = MFCreateMediaType(&pPCMType);
    assert(SUCCEEDED(result));
    result = pPCMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    assert(SUCCEEDED(result));
    result = pPCMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    assert(SUCCEEDED(result));

    result = pReader->SetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        nullptr,
        pPCMType.Get());
    assert(SUCCEEDED(result));

    // 実際にセットされたメディアタイプを取得
    ComPtr<IMFMediaType> pOutType;
    result = pReader->GetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        &pOutType);
    assert(SUCCEEDED(result));

    // WaveFormat取得
    WAVEFORMATEX* waveFormat = nullptr;
    result = MFCreateWaveFormatExFromMFMediaType(pOutType.Get(), &waveFormat, nullptr);
    assert(SUCCEEDED(result));

    SoundData soundData{};
    soundData.wfex = *waveFormat;

    // 用済みなので解放（MFが確保したメモリ）
    CoTaskMemFree(waveFormat);

    // （任意）reserveで高速化：とりあえず無しでもOK

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
        assert(SUCCEEDED(result));

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }

        if (pSample) {
            ComPtr<IMFMediaBuffer> pBuffer;
            result = pSample->ConvertToContiguousBuffer(&pBuffer);
            assert(SUCCEEDED(result));

            BYTE* pData = nullptr;
            DWORD maxLength = 0;
            DWORD currentLength = 0;

            result = pBuffer->Lock(&pData, &maxLength, &currentLength);
            assert(SUCCEEDED(result));

            // ★末尾に追加
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
