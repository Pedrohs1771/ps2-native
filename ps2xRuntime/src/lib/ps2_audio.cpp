#include "runtime/ps2_audio.h"
#include "runtime/ps2_memory.h"
#include "ps2_host_backend.h"
#include "ps2_log.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <vector>

namespace
{
    std::vector<uint8_t> buildWavFromPcm(const int16_t *pcm, size_t sampleCount, uint32_t sampleRate)
    {
        const uint32_t dataSize = static_cast<uint32_t>(sampleCount * 2);
        const uint32_t fileSize = 36 + dataSize;
        std::vector<uint8_t> wav(8 + fileSize);

        uint8_t *p = wav.data();
        p[0] = 'R';
        p[1] = 'I';
        p[2] = 'F';
        p[3] = 'F';
        p[4] = static_cast<uint8_t>(fileSize);
        p[5] = static_cast<uint8_t>(fileSize >> 8);
        p[6] = static_cast<uint8_t>(fileSize >> 16);
        p[7] = static_cast<uint8_t>(fileSize >> 24);
        p[8] = 'W';
        p[9] = 'A';
        p[10] = 'V';
        p[11] = 'E';
        p[12] = 'f';
        p[13] = 'm';
        p[14] = 't';
        p[15] = ' ';
        p[16] = 16;
        p[17] = 0;
        p[18] = 0;
        p[19] = 0;
        p[20] = 1;
        p[21] = 0;
        p[22] = 1;
        p[23] = 0;
        p[24] = static_cast<uint8_t>(sampleRate);
        p[25] = static_cast<uint8_t>(sampleRate >> 8);
        p[26] = static_cast<uint8_t>(sampleRate >> 16);
        p[27] = static_cast<uint8_t>(sampleRate >> 24);
        const uint32_t byteRate = sampleRate * 2;
        p[28] = static_cast<uint8_t>(byteRate);
        p[29] = static_cast<uint8_t>(byteRate >> 8);
        p[30] = static_cast<uint8_t>(byteRate >> 16);
        p[31] = static_cast<uint8_t>(byteRate >> 24);
        p[32] = 2;
        p[33] = 0;
        p[34] = 16;
        p[35] = 0;
        p[36] = 'd';
        p[37] = 'a';
        p[38] = 't';
        p[39] = 'a';
        p[40] = static_cast<uint8_t>(dataSize);
        p[41] = static_cast<uint8_t>(dataSize >> 8);
        p[42] = static_cast<uint8_t>(dataSize >> 16);
        p[43] = static_cast<uint8_t>(dataSize >> 24);
        std::memcpy(p + 44, pcm, dataSize);
        return wav;
    }
}

namespace ps2_vag
{
    bool decode(const uint8_t *data, uint32_t sizeBytes,
                std::vector<int16_t> &outPcm, uint32_t &outSampleRate);
}

struct PS2AudioBackend::Impl
{
    struct BlockTransfer
    {
        std::vector<uint8_t> data;
        uint32_t baseAddress = 0;
        uint32_t frame = 0;
        bool active = false;
        bool loop = false;
        bool hardwareDma = false;
        AudioStream stream{};
        int callbackSlot = -1;
    };
    struct CallbackTarget
    {
        PS2AudioBackend *backend = nullptr;
        uint32_t core = 0;
    };
    // raylib's callback has no userdata. Two independently owned slots cover
    // the two SPU2 cores; headless decoders do not reserve a device slot.
    inline static std::array<std::atomic<CallbackTarget *>, 2> callbacks{};
    std::array<CallbackTarget, 2> callbackTargets{};
    std::array<BlockTransfer, 2> blockTransfers{};
    std::array<std::array<uint16_t, 0x23>, 2> params{};
    std::array<std::array<std::array<uint16_t, 8>, 24>, 2> voiceParams{};
    mutable std::mutex blockMutex;
    std::atomic<uint32_t> soundCommandTraceCount{0u};

    static void callback(unsigned slot, void *buffer, unsigned frames)
    {
        auto *target = callbacks[slot].load(std::memory_order_acquire);
        if (target)
            target->backend->renderBlockAudio(target->core, static_cast<int16_t *>(buffer), frames);
        else
            std::memset(buffer, 0, frames * 2u * sizeof(int16_t));
    }
    static void callback0(void *buffer, unsigned frames) { callback(0, buffer, frames); }
    static void callback1(void *buffer, unsigned frames) { callback(1, buffer, frames); }

    struct TrackedSound
    {
        Sound snd;
        uint32_t sampleKey;
    };
    std::vector<TrackedSound> activeSounds;
};

PS2AudioBackend::PS2AudioBackend() : m_impl(std::make_unique<Impl>())
{
}

PS2AudioBackend::~PS2AudioBackend()
{
    if (m_impl)
        stopAll();
}

bool PS2AudioBackend::startBlockTransfer(uint32_t core, const uint8_t *data, uint32_t sizeBytes,
                                        uint32_t baseAddress, uint32_t startOffset, bool loop)
{
    if (core >= 2u || !data || sizeBytes < 1024u || (sizeBytes & 1023u) != 0u ||
        (loop && (sizeBytes & 2047u) != 0u) || startOffset >= sizeBytes || (startOffset & 1023u) != 0u)
        return false;
    stopBlockTransfer(core);
    auto &transfer = m_impl->blockTransfers[core];
    {
        std::lock_guard<std::mutex> lock(m_impl->blockMutex);
        transfer.data.assign(data, data + sizeBytes);
        transfer.baseAddress = baseAddress & 0x00FFFFFFu;
        transfer.frame = startOffset / 4u;
        transfer.active = true;
        transfer.loop = loop;
        transfer.hardwareDma = false;
    }
    return startBlockDevice(core);
}

bool PS2AudioBackend::startBlockDevice(uint32_t core)
{
    auto &transfer = m_impl->blockTransfers[core];
#if !defined(PLATFORM_VITA)
    if (m_audioReady && !transfer.stream.buffer)
    {
        auto &target = m_impl->callbackTargets[core];
        target = {this, core};
        int slot = -1;
        for (int candidate = 0; candidate < 2; ++candidate)
        {
            Impl::CallbackTarget *empty = nullptr;
            if (Impl::callbacks[candidate].compare_exchange_strong(empty, &target))
            {
                slot = candidate;
                break;
            }
        }
        if (slot < 0)
        {
            stopBlockTransfer(core);
            return false;
        }
        transfer.stream = LoadAudioStream(48000u, 16u, 2u);
        if (!transfer.stream.buffer)
        {
            Impl::callbacks[slot].store(nullptr, std::memory_order_release);
            stopBlockTransfer(core);
            return false;
        }
        transfer.callbackSlot = slot;
        SetAudioStreamCallback(transfer.stream, slot == 0 ? Impl::callback0 : Impl::callback1);
        PlayAudioStream(transfer.stream);
    }
#endif
    return true;
}

void PS2AudioBackend::refreshBlockTransfer(uint32_t core, const uint8_t *data, uint32_t sizeBytes)
{
    if (core >= 2u || !data)
        return;
    std::lock_guard<std::mutex> lock(m_impl->blockMutex);
    auto &transfer = m_impl->blockTransfers[core];
    if (transfer.active && transfer.data.size() == sizeBytes)
        std::memcpy(transfer.data.data(), data, sizeBytes);
}

uint32_t PS2AudioBackend::blockTransferStatus(uint32_t core) const
{
    if (core >= 2u)
        return 0u;
    std::lock_guard<std::mutex> lock(m_impl->blockMutex);
    const auto &transfer = m_impl->blockTransfers[core];
    if (!transfer.active)
        return 0u;
    const uint32_t offset = (transfer.frame / 256u) * 1024u;
    const uint32_t bank = transfer.loop && offset >= transfer.data.size() / 2u ? 1u : 0u;
    return (bank << 24u) | ((transfer.baseAddress + offset) & 0x00FFFFFFu);
}

void PS2AudioBackend::stopBlockTransfer(uint32_t core)
{
    if (core >= 2u)
        return;
    auto &transfer = m_impl->blockTransfers[core];
    // Never hold blockMutex while taking raylib's device lock: its mixer calls
    // renderBlockAudio with the opposite lock order.
#if !defined(PLATFORM_VITA)
    if (transfer.stream.buffer)
    {
        StopAudioStream(transfer.stream);
        UnloadAudioStream(transfer.stream);
        transfer.stream = {};
    }
#endif
    if (transfer.callbackSlot >= 0)
    {
        Impl::callbacks[transfer.callbackSlot].store(nullptr, std::memory_order_release);
        transfer.callbackSlot = -1;
    }
    std::lock_guard<std::mutex> lock(m_impl->blockMutex);
    transfer.active = false;
    transfer.hardwareDma = false;
    transfer.frame = 0u;
    transfer.baseAddress = 0u;
    transfer.data.clear();
}

void PS2AudioBackend::renderBlockAudio(uint32_t core, int16_t *stereoPcm, uint32_t frames)
{
    if (!stereoPcm)
        return;
    std::fill_n(stereoPcm, static_cast<size_t>(frames) * 2u, 0);
    if (core >= 2u)
        return;
    std::lock_guard<std::mutex> lock(m_impl->blockMutex);
    auto &transfer = m_impl->blockTransfers[core];
    auto &params = m_impl->params[core];
    for (uint32_t frame = 0; frame < frames && transfer.active; ++frame)
    {
        const uint32_t tile = (transfer.frame / 256u) * 1024u;
        const uint32_t position = (transfer.frame % 256u) * 2u;
        for (uint32_t channel = 0; channel < 2u; ++channel)
        {
            int16_t sample = 0;
            std::memcpy(&sample, transfer.data.data() + tile + channel * 512u + position, sizeof(sample));
            // BlockTrans AutoDMA is routed through the core B input bus.
            // Core A parameters control external input and must not gate this stream.
            const int16_t inputVolume = static_cast<int16_t>(params[0x0Fu + channel]);
            // Direct master volume is a signed 15-bit value; sweep modes need
            // their own envelope contract and are intentionally not fabricated.
            const uint16_t masterRaw = params[0x09u + channel];
            const int32_t masterVolume = (masterRaw & 0x8000u) ? 0 :
                ((masterRaw & 0x4000u) ? static_cast<int32_t>(masterRaw) - 0x8000 : masterRaw);
            const int64_t mixed = static_cast<int64_t>(sample) * inputVolume * masterVolume / (0x7FFFll * 0x3FFFll);
            stereoPcm[frame * 2u + channel] = static_cast<int16_t>(std::clamp<int64_t>(mixed, -32768, 32767));
        }
        ++transfer.frame;
        const size_t availableFrames = transfer.hardwareDma ?
            (transfer.data.size() / 1024u) * 256u : transfer.data.size() / 4u;
        if (transfer.frame >= availableFrames)
        {
            if (transfer.loop)
                transfer.frame = 0u;
            else
                transfer.active = false;
        }
    }
}

bool PS2AudioBackend::enqueueSpu2Pcm(uint32_t core, std::span<const uint8_t> data)
{
    constexpr size_t maxQueuedBytes = 2u * 1024u * 1024u;
    if (core >= 2u || data.empty() || (data.size() & 3u) != 0u || data.size() > maxQueuedBytes)
        return false;
    bool hardwareDma;
    {
        std::lock_guard<std::mutex> lock(m_impl->blockMutex);
        hardwareDma = m_impl->blockTransfers[core].hardwareDma;
    }
    if (!hardwareDma)
        stopBlockTransfer(core);
    {
        std::lock_guard<std::mutex> lock(m_impl->blockMutex);
        auto &transfer = m_impl->blockTransfers[core];
        // Reclaim whole consumed stereo tiles; preserve a partially consumed
        // tile and incomplete incoming planes. All guest bytes are copied.
        const size_t consumed = (transfer.frame / 256u) * 1024u;
        if (consumed)
        {
            transfer.data.erase(transfer.data.begin(), transfer.data.begin() + consumed);
            transfer.frame -= static_cast<uint32_t>(consumed / 4u);
        }
        if (data.size() > maxQueuedBytes - transfer.data.size())
            return false;
        transfer.data.insert(transfer.data.end(), data.begin(), data.end());
        transfer.hardwareDma = true;
        transfer.loop = false;
        transfer.active = transfer.frame < (transfer.data.size() / 1024u) * 256u;
    }
    return startBlockDevice(core);
}

void PS2AudioBackend::writeSpu2Register(uint32_t address, uint16_t value)
{
    // Physical register layout follows PS2SDK's spu2regs.h. The mixer/master
    // bank has a 40-byte core stride; the voice bank has a 0x400-byte stride.
    address &= 0x1FFFFFFFu;
    if (address < 0x1F900000u || address >= 0x1F900800u || (address & 1u))
        return;
    const uint32_t offset = address - 0x1F900000u;
    for (uint32_t core = 0u; core < 2u; ++core)
    {
        const uint32_t master = 0x760u + core * 40u;
        if (offset >= master && offset < master + 20u)
        {
            setSpu2Param(static_cast<uint16_t>(((9u + (offset - master) / 2u) << 8u) | 0x80u | core), value);
            return;
        }
    }
    const uint32_t core = offset / 0x400u;
    const uint32_t local = offset % 0x400u;
    if (local < 0x180u)
        setSpu2Param(static_cast<uint16_t>(((local % 16u) / 2u << 8u) | (local / 16u << 1u) | core), value);
    else if (local == 0x198u)
        setSpu2Param(static_cast<uint16_t>(0x0800u | core), value);
    else if (local == 0x1B0u && (value & (1u << core)) == 0u)
    {
        bool hardwareDma;
        {
            std::lock_guard<std::mutex> lock(m_impl->blockMutex);
            hardwareDma = m_impl->blockTransfers[core].hardwareDma;
        }
        if (hardwareDma)
            stopBlockTransfer(core);
    }
}

void PS2AudioBackend::resetSpu2()
{
    stopAll();
    std::lock_guard<std::mutex> lock(m_impl->blockMutex);
    m_impl->params = {};
    m_impl->voiceParams = {};
}

void PS2AudioBackend::setSpu2Param(uint16_t entry, uint16_t value)
{
    const uint32_t index = entry >> 8u;
    if (index >= 0x23u)
        return;
    std::lock_guard<std::mutex> lock(m_impl->blockMutex);
    if (index < 8u)
    {
        const uint32_t voice = (entry >> 1u) & 0x1Fu;
        if (voice < 24u)
            m_impl->voiceParams[entry & 1u][voice][index] = value;
    }
    else
        m_impl->params[entry & 1u][index] = value;
}

uint16_t PS2AudioBackend::getSpu2Param(uint16_t entry) const
{
    const uint32_t index = entry >> 8u;
    if (index >= 0x23u)
        return 0u;
    std::lock_guard<std::mutex> lock(m_impl->blockMutex);
    if (index < 8u)
    {
        const uint32_t voice = (entry >> 1u) & 0x1Fu;
        return voice < 24u ? m_impl->voiceParams[entry & 1u][voice][index] : 0u;
    }
    return m_impl->params[entry & 1u][index];
}

void PS2AudioBackend::onVagTransfer(const uint8_t *rdram, uint32_t srcAddr, uint32_t sizeBytes)
{
    if (!rdram || sizeBytes < 48)
        return;

    const uint32_t physAddr = srcAddr & PS2_RAM_MASK;
    if (physAddr + sizeBytes > PS2_RAM_SIZE)
        return;

    std::vector<int16_t> pcm;
    uint32_t sampleRate = 44100;
    if (!ps2_vag::decode(rdram + physAddr, sizeBytes, pcm, sampleRate))
        return;

    std::lock_guard<std::mutex> lock(m_mutex);
    DecodedSample sample;
    sample.pcm = std::move(pcm);
    sample.sampleRate = sampleRate;
    m_sampleBank[physAddr] = std::move(sample);
    m_mostRecentSampleKey = physAddr;
}

void PS2AudioBackend::onVagTransferFromBuffer(const uint8_t *data, uint32_t sizeBytes, uint32_t keyAddr)
{
    if (!data || sizeBytes < 48)
        return;

    std::vector<int16_t> pcm;
    uint32_t sampleRate = 44100;
    if (!ps2_vag::decode(data, sizeBytes, pcm, sampleRate))
        return;

    const uint32_t physAddr = keyAddr & PS2_RAM_MASK;
    std::lock_guard<std::mutex> lock(m_mutex);
    DecodedSample sample;
    sample.pcm = std::move(pcm);
    sample.sampleRate = sampleRate;
    m_sampleBank[physAddr] = sample;
    m_mostRecentSampleKey = physAddr;
    m_loadOrderSamples.push_back(std::move(sample));
    m_loadOrderSampleKeys.push_back(physAddr);
    constexpr size_t kMaxLoadOrderSamples = 32;
    if (m_loadOrderSamples.size() > kMaxLoadOrderSamples)
    {
        m_loadOrderSamples.erase(m_loadOrderSamples.begin());
        m_loadOrderSampleKeys.erase(m_loadOrderSampleKeys.begin());
    }
}

void PS2AudioBackend::onSoundCommand(uint32_t sid, uint32_t rpcNum,
                                     const uint8_t *sendBuf, uint32_t sendSize,
                                     uint8_t *recvBuf, uint32_t recvSize)
{
    if (sid != 0x80000701u)
        return;

    const uint32_t traceIndex = m_impl->soundCommandTraceCount.fetch_add(1u, std::memory_order_relaxed);
    if (traceIndex < 32u)
    {
        PS2_IF_AGRESSIVE_LOGS({
            uint32_t nonzeroBytes = 0u;
            for (uint32_t i = 0u; sendBuf && i < sendSize; ++i)
                nonzeroBytes += sendBuf[i] != 0u;
            std::cerr << "[Audio:LibSdRpc] function=0x" << std::hex << rpcNum
                      << std::dec << " sendBytes=" << sendSize
                      << " nonzeroBytes=" << nonzeroBytes
                      << " receiveBytes=" << recvSize << std::endl;
        });
    }

    // libsdr's standard request starts with an EE return address, then entry
    // and value. SetParam is register configuration; status/callback commands
    // must never start whichever VAG happened to be loaded last.
    if ((rpcNum == 0x8010u || rpcNum == 0x8020u) && sendBuf && sendSize >= 12u)
    {
        uint32_t entry = 0, value = 0;
        std::memcpy(&entry, sendBuf + 4u, sizeof(entry));
        std::memcpy(&value, sendBuf + 8u, sizeof(value));
        if (rpcNum == 0x8010u)
            setSpu2Param(static_cast<uint16_t>(entry), static_cast<uint16_t>(value));
        else
            value = getSpu2Param(static_cast<uint16_t>(entry));
        if (recvBuf && recvSize >= sizeof(value))
        {
            const uint32_t result = rpcNum == 0x8010u ? 0u : value;
            std::memcpy(recvBuf, &result, sizeof(result));
        }
    }
}

void PS2AudioBackend::play(uint32_t sampleAddr, float pitch, float volume, uint32_t voiceIndex)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    DecodedSample *sampleToPlay = nullptr;
    uint32_t sampleKey = 0;

    auto it = m_sampleBank.find(sampleAddr & PS2_RAM_MASK);
    if (it != m_sampleBank.end())
    {
        sampleToPlay = &it->second;
        sampleKey = it->first;
    }
    else if (voiceIndex != 0xFFFFFFFFu &&
             voiceIndex < m_loadOrderSamples.size() &&
             voiceIndex < m_loadOrderSampleKeys.size())
    {
        sampleToPlay = &m_loadOrderSamples[voiceIndex];
        sampleKey = m_loadOrderSampleKeys[voiceIndex];
    }
    else
    {
        it = m_sampleBank.find(m_mostRecentSampleKey);
        if (it == m_sampleBank.end())
            return;
        sampleToPlay = &it->second;
        sampleKey = it->first;
    }
    if (!sampleToPlay || sampleToPlay->pcm.empty())
        return;

    const bool isBgm = (sampleToPlay->pcm.size() > static_cast<size_t>(sampleToPlay->sampleRate * 5));
    playDecodedSample(sampleKey, *sampleToPlay, pitch, volume, isBgm);
}

void PS2AudioBackend::pruneFinishedSounds()
{
#if defined(PLATFORM_VITA)
    return;
#else
    auto &sounds = m_impl->activeSounds;
    auto it = sounds.begin();
    while (it != sounds.end())
    {
        if (!IsSoundPlaying(it->snd))
        {
            UnloadSound(it->snd);
            it = sounds.erase(it);
        }
        else
        {
            ++it;
        }
    }
#endif
}

void PS2AudioBackend::playDecodedSample(uint32_t sampleKey, DecodedSample &sample, float pitch, float volume,
                                        bool isBgm)
{
#if defined(PLATFORM_VITA)
    (void)sampleKey;
    (void)sample;
    (void)pitch;
    (void)volume;
    (void)isBgm;
    return;
#else
    if (!m_audioReady || sample.pcm.empty())
        return;

    pruneFinishedSounds();

    for (const auto &t : m_impl->activeSounds)
    {
        if (t.sampleKey == sampleKey && IsSoundPlaying(t.snd))
            return;
    }

    auto &sounds = m_impl->activeSounds;
    if (isBgm)
    {
        for (auto it = sounds.begin(); it != sounds.end();)
        {
            if (IsSoundPlaying(it->snd))
            {
                StopSound(it->snd);
                UnloadSound(it->snd);
                it = sounds.erase(it);
            }
            else
                ++it;
        }
    }

    constexpr int kMaxConcurrentSounds = 4;
    while (static_cast<int>(sounds.size()) >= kMaxConcurrentSounds)
    {
        StopSound(sounds.front().snd);
        UnloadSound(sounds.front().snd);
        sounds.erase(sounds.begin());
    }

    std::vector<uint8_t> wav = buildWavFromPcm(sample.pcm.data(), sample.pcm.size(), sample.sampleRate);
    Wave wave = LoadWaveFromMemory(".wav", wav.data(), static_cast<int>(wav.size()));
    if (wave.frameCount <= 0)
        return;
    Sound snd = LoadSoundFromWave(wave);
    UnloadWave(wave);
    SetSoundPitch(snd, pitch);
    SetSoundVolume(snd, volume);
    m_impl->activeSounds.push_back({snd, sampleKey});
    PlaySound(snd);
#endif
}

void PS2AudioBackend::stop(uint32_t voiceId)
{
    (void)voiceId;
}

void PS2AudioBackend::stopAll()
{
    stopBlockTransfer(0u);
    stopBlockTransfer(1u);
    std::lock_guard<std::mutex> lock(m_mutex);
#if defined(PLATFORM_VITA)
    return;
#else
    for (auto &t : m_impl->activeSounds)
    {
        StopSound(t.snd);
        UnloadSound(t.snd);
    }
    m_impl->activeSounds.clear();
#endif
}
