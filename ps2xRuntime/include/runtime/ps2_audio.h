#ifndef PS2_AUDIO_H
#define PS2_AUDIO_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <span>

class PS2AudioBackend
{
public:
    PS2AudioBackend();
    ~PS2AudioBackend();

    void onVagTransfer(const uint8_t *rdram, uint32_t srcAddr, uint32_t sizeBytes);
    void onVagTransferFromBuffer(const uint8_t *data, uint32_t sizeBytes, uint32_t keyAddr);
    void onSoundCommand(uint32_t sid, uint32_t rpcNum,
                        const uint8_t *sendBuf, uint32_t sendSize,
                        uint8_t *recvBuf, uint32_t recvSize);

    // SPU2 AutoDMA consumes signed 16-bit PCM: 256 left samples followed
    // by 256 right samples, repeated. Status reads never clock this stream.
    bool startBlockTransfer(uint32_t core, const uint8_t *data, uint32_t sizeBytes,
                            uint32_t baseAddress, uint32_t startOffset, bool loop);
    void refreshBlockTransfer(uint32_t core, const uint8_t *data, uint32_t sizeBytes);
    uint32_t blockTransferStatus(uint32_t core) const;
    void stopBlockTransfer(uint32_t core);
    void renderBlockAudio(uint32_t core, int16_t *stereoPcm, uint32_t frames);
    void setSpu2Param(uint16_t entry, uint16_t value);
    uint16_t getSpu2Param(uint16_t entry) const;
    void writeSpu2Register(uint32_t address, uint16_t value);
    bool enqueueSpu2Pcm(uint32_t core, std::span<const uint8_t> data);
    void resetSpu2();

    void play(uint32_t sampleAddr, float pitch = 1.0f, float volume = 1.0f,
              uint32_t voiceIndex = 0xFFFFFFFFu);
    void stop(uint32_t voiceId);
    void stopAll();
    void setAudioReady(bool ready) { m_audioReady = ready; }

private:
    struct DecodedSample
    {
        std::vector<int16_t> pcm;
        uint32_t sampleRate = 44100;
    };

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_audioReady = false;
    uint32_t m_mostRecentSampleKey = 0;
    std::vector<DecodedSample> m_loadOrderSamples;
    std::vector<uint32_t> m_loadOrderSampleKeys;
    std::unordered_map<uint32_t, DecodedSample> m_sampleBank;
    std::mutex m_mutex;

    void playDecodedSample(uint32_t sampleKey, DecodedSample &sample, float pitch, float volume,
                          bool isBgm = false);
    void pruneFinishedSounds();
    bool startBlockDevice(uint32_t core);
};

#endif
