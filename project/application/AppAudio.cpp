#include "AppAudio.h"
#include <fstream>
#include <cassert>

bool AppAudio::Initialize() {
    HRESULT hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) return false;

    hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
    if (FAILED(hr)) return false;

    return true;
}

void AppAudio::Finalize() {
    if (masterVoice_) {
        masterVoice_->DestroyVoice();
        masterVoice_ = nullptr;
    }
    xAudio2_.Reset();
}

SoundData AppAudio::LoadWave(const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    assert(file);

    SoundData soundData{};

    // RIFF header
    char riff[4];
    file.read(riff, 4);
    assert(std::strncmp(riff, "RIFF", 4) == 0);

    file.seekg(22);
    file.read(reinterpret_cast<char*>(&soundData.wfex.nChannels), 2);
    file.read(reinterpret_cast<char*>(&soundData.wfex.nSamplesPerSec), 4);

    file.seekg(34);
    file.read(reinterpret_cast<char*>(&soundData.wfex.wBitsPerSample), 2);

    file.seekg(40);
    file.read(reinterpret_cast<char*>(&soundData.bufferSize), 4);

    soundData.pBuffer = new BYTE[soundData.bufferSize];
    file.read(reinterpret_cast<char*>(soundData.pBuffer), soundData.bufferSize);

    return soundData;
}

void AppAudio::Unload(SoundData* soundData) {
    delete[] soundData->pBuffer;
    soundData->pBuffer = nullptr;
    soundData->bufferSize = 0;
}

void AppAudio::PlayWave(const SoundData& soundData) {
    if (!xAudio2_) {
        return;
    }

    if (soundData.pBuffer == nullptr || soundData.bufferSize == 0) {
        return;
    }

    IXAudio2SourceVoice* sourceVoice = nullptr;
    HRESULT hr = xAudio2_->CreateSourceVoice(&sourceVoice, &soundData.wfex);
    if (FAILED(hr) || sourceVoice == nullptr) {
        return;
    }

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    hr = sourceVoice->SubmitSourceBuffer(&buf);
    if (FAILED(hr)) {
        sourceVoice->DestroyVoice();
        return;
    }

    hr = sourceVoice->Start();
    if (FAILED(hr)) {
        sourceVoice->DestroyVoice();
        return;
    }
}