#pragma once
#include <xaudio2.h>
#include <wrl.h>
#include <cstdint>

struct ChunkHeader {
    char id[4];
    int32_t size;
};

struct RiffHeader {
    ChunkHeader chunk;
    char type[4];
};

struct FormatChunk {
    ChunkHeader chunk;
    WAVEFORMATEX fmt;
};

struct SoundData {
    WAVEFORMATEX wfex{};
    BYTE* pBuffer = nullptr;
    uint32_t bufferSize = 0;
};

class AppAudio {
public:
    bool Initialize();
    void Finalize();

    SoundData LoadWave(const char* filename);
    void Unload(SoundData* soundData);
    void PlayWave(const SoundData& soundData);

private:
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_ = nullptr;
};