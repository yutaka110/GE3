#pragma once

#include <Windows.h>
#include <xaudio2.h>

struct SoundData {
    WAVEFORMATEX wfex;
    BYTE* pBuffer;
    unsigned int bufferSize;
};

SoundData SoundLoadWave(const char* filename);
void SoundUnload(SoundData* soundData);
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData);