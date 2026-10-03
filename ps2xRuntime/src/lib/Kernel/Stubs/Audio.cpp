#include "Common.h"
#include "Audio.h"

namespace ps2_stubs
{
    namespace
    {
        constexpr uint32_t kLibSdCmdSetParam = 0x8010u;
        constexpr uint32_t kLibSdCmdVoiceTrans = 0x80D0u;
        constexpr uint32_t kLibSdCmdBlockTrans = 0x80E0u;
        constexpr uint32_t kLibSdCmdVoiceTransStatus = 0x80F0u;
        constexpr uint32_t kLibSdCmdBlockTransStatus = 0x8100u;
        constexpr uint32_t kAudioPositionMask = 0x00FFFFFFu;
        constexpr uint32_t kLibSdCoreCount = 2u;
        constexpr uint32_t kLibSdTransModeIo = 0x08u;
        constexpr uint32_t kLibSdTransDirectionMask = 0x03u;
        constexpr uint32_t kLibSdTransStop = 0x02u;
        constexpr uint32_t kLibSdTransLoop = 0x10u;

        struct VoiceTransferState
        {
            uint32_t sourceAddress = 0u;
            uint32_t destinationAddress = 0u;
            uint32_t size = 0u;
            uint16_t mode = 0u;
            bool completed = true;
        };

        struct BlockTransferState
        {
            uint32_t base = 0u;
            uint32_t size = 0u;
            uint32_t pauseBase = 0u;
            uint32_t offset = 0u;
            uint32_t statusTraceCount = 0u;
            uint32_t pcmStatusSampleCount = 0u;
            uint32_t lastPcmNonzeroBytes = 0u;
            uint16_t mode = 0u;
            bool active = false;
            bool loop = false;
            std::vector<uint8_t> snapshot;
        };

        struct AudioStubState
        {
            bool initialized = false;
            std::array<VoiceTransferState, kLibSdCoreCount> voiceTransfers{};
            std::array<BlockTransferState, kLibSdCoreCount> blockTransfers{};
            uint32_t paramTraceCount = 0u;
            uint32_t transToIopTraceCount = 0u;
            uint32_t pcmTraceCount = 0u;
        };

        std::mutex g_audio_stub_mutex;
        AudioStubState g_audio_stub_state;

        void resetAudioStubStateUnlocked()
        {
            g_audio_stub_state = {};
        }

        uint32_t currentBlockStatus(const BlockTransferState &transfer)
        {
            const uint32_t position = (transfer.base + transfer.offset) & kAudioPositionMask;
            const uint32_t halfSize = transfer.size / 2u;
            const uint32_t bank = (transfer.loop && halfSize != 0u && transfer.offset >= halfSize) ? 1u : 0u;
            return (bank << 24u) | position;
        }
    }

    void resetAudioStubState()
    {
        std::lock_guard<std::mutex> lock(g_audio_stub_mutex);
        resetAudioStubStateUnlocked();
    }

    void sceSdCallBack(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSdCallBack", rdram, ctx, runtime);
    }

    void sceSdRemote(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t cmd = getRegU32(ctx, 5);
        const uint32_t cmdArg0 = getRegU32(ctx, 6);
        const uint32_t cmdArg1 = getRegU32(ctx, 7);
        // The EE n32 ABI carries these variadic arguments in a4-a6. Zero is
        // a valid argument, not permission to substitute unrelated stack data.
        const uint32_t arg4 = getRegU32(ctx, 8);
        const uint32_t arg5 = getRegU32(ctx, 9);
        const uint32_t arg6 = getRegU32(ctx, 10);

        std::lock_guard<std::mutex> lock(g_audio_stub_mutex);
        g_audio_stub_state.initialized = true;
        const uint32_t core = cmdArg0 & (kLibSdCoreCount - 1u);
        VoiceTransferState &voiceTransfer = g_audio_stub_state.voiceTransfers[core];
        BlockTransferState &blockTransfer = g_audio_stub_state.blockTransfers[core];
        uint32_t returnValue = 0u;

        if (cmd == kLibSdCmdVoiceTrans)
        {
            voiceTransfer.sourceAddress = arg4;
            voiceTransfer.destinationAddress = arg5;
            voiceTransfer.size = arg6;
            voiceTransfer.mode = static_cast<uint16_t>(cmdArg1);
            voiceTransfer.completed = true;

            // sceSdVoiceTrans returns the transferred byte count for DMA
            // mode and zero for its synchronous programmed-I/O mode.
            returnValue = ((voiceTransfer.mode & kLibSdTransModeIo) != 0u)
                              ? 0u
                              : voiceTransfer.size;

            PS2_IF_AGRESSIVE_LOGS({
                std::cerr << "[Audio:VoiceTrans] core=" << core
                          << " src=0x" << std::hex << voiceTransfer.sourceAddress
                          << " dest=0x" << voiceTransfer.destinationAddress
                          << " size=0x" << voiceTransfer.size
                          << " mode=0x" << voiceTransfer.mode
                          << std::dec << std::endl;
            });
        }
        else if (cmd == kLibSdCmdBlockTrans)
        {
            const uint32_t direction = cmdArg1 & kLibSdTransDirectionMask;
            if (direction == kLibSdTransStop)
            {
                returnValue = runtime ? runtime->audioBackend().blockTransferStatus(core) : currentBlockStatus(blockTransfer);
                if (runtime)
                    runtime->audioBackend().stopBlockTransfer(core);
                blockTransfer = {};
            }
            else if (arg4 != 0u && arg5 != 0u)
            {
                blockTransfer.base = arg4 & kAudioPositionMask;
                blockTransfer.size = arg5;
                blockTransfer.pauseBase =
                    ((arg6 != 0u) ? arg6 : arg4) & kAudioPositionMask;
                blockTransfer.mode = static_cast<uint16_t>(cmdArg1);
                blockTransfer.loop = (cmdArg1 & kLibSdTransLoop) != 0u;

                const uint32_t pauseOffset =
                    (blockTransfer.pauseBase - blockTransfer.base) & kAudioPositionMask;
                blockTransfer.offset = (pauseOffset < blockTransfer.size) ? pauseOffset : 0u;
                blockTransfer.statusTraceCount = 0u;
                blockTransfer.active = true;
                returnValue = 0u;
                if (runtime)
                {
                    // Block DMA reads the IOP address space, not EE RDRAM.
                    if (direction == 1u || !runtime->isIopMemoryRange(blockTransfer.base, blockTransfer.size))
                    {
                        blockTransfer = {};
                        returnValue = std::numeric_limits<uint32_t>::max();
                    }
                    else
                    {
                        blockTransfer.snapshot.resize(blockTransfer.size);
                        const bool copied = runtime->readIopMemory(blockTransfer.base, blockTransfer.snapshot.data(), blockTransfer.size);
                        const bool started = copied && runtime->audioBackend().startBlockTransfer(
                            core, blockTransfer.snapshot.data(), blockTransfer.size,
                            blockTransfer.base, blockTransfer.offset, blockTransfer.loop);
                        if (!started)
                        {
                            blockTransfer = {};
                            returnValue = std::numeric_limits<uint32_t>::max();
                        }
                    }
                }
            }
            else
            {
                returnValue = std::numeric_limits<uint32_t>::max();
            }

            PS2_IF_AGRESSIVE_LOGS({
                std::cerr << "[Audio:BlockTrans] core=" << core
                          << " active=" << blockTransfer.active
                          << " base=0x" << std::hex << blockTransfer.base
                          << " size=0x" << blockTransfer.size
                          << " pause=0x" << blockTransfer.pauseBase
                          << " offset=0x" << blockTransfer.offset
                          << std::dec << std::endl;
            });
        }
        else if (cmd == kLibSdCmdVoiceTransStatus)
        {
            returnValue = voiceTransfer.completed ? 1u : 0u;
        }
        else if (cmd == kLibSdCmdBlockTransStatus)
        {
            if (blockTransfer.active && blockTransfer.size != 0u)
            {
                if (runtime && runtime->readIopMemory(blockTransfer.base, blockTransfer.snapshot.data(), blockTransfer.size))
                {
                    runtime->audioBackend().refreshBlockTransfer(core, blockTransfer.snapshot.data(), blockTransfer.size);
                    const uint32_t sampleIndex = blockTransfer.pcmStatusSampleCount++;
                    static const bool traceEveryPcmPoll = []
                    {
                        const char *value = std::getenv("PS2X_TRACE_AUDIO_PCM");
                        return value != nullptr && std::strcmp(value, "0") != 0;
                    }();
                    if (traceEveryPcmPoll || sampleIndex < 4u || (sampleIndex % 120u) == 0u)
                    {
                        PS2_IF_AGRESSIVE_LOGS({
                            uint32_t nonzero = 0u;
                            uint32_t nonzeroSamples = 0u;
                            uint32_t peak = 0u;
                            for (size_t byteIndex = 0u; byteIndex + 1u < blockTransfer.snapshot.size(); byteIndex += 2u)
                            {
                                const uint8_t lo = blockTransfer.snapshot[byteIndex];
                                const uint8_t hi = blockTransfer.snapshot[byteIndex + 1u];
                                nonzero += (lo != 0u) + (hi != 0u);
                                int16_t sample = 0;
                                std::memcpy(&sample, blockTransfer.snapshot.data() + byteIndex, sizeof(sample));
                                if (sample != 0)
                                {
                                    ++nonzeroSamples;
                                    peak = std::max(peak, static_cast<uint32_t>(std::abs(static_cast<int32_t>(sample))));
                                }
                            }
                            const uint32_t status = runtime->audioBackend().blockTransferStatus(core);
                            const bool transitioned = (blockTransfer.lastPcmNonzeroBytes == 0u) != (nonzero == 0u);
                            if (g_audio_stub_state.pcmTraceCount < 128u &&
                                (sampleIndex < 4u || (sampleIndex % 120u) == 0u || transitioned))
                            {
                                std::cerr << "[Audio:IOPPCM] poll=" << sampleIndex
                                          << " core=" << core
                                          << " base=0x" << std::hex << blockTransfer.base
                                          << " status=0x" << status
                                          << " bytes=0x" << blockTransfer.size
                                          << std::dec << " nonzeroBytes=" << nonzero
                                          << " nonzeroSamples=" << nonzeroSamples
                                          << " peak=" << peak << std::endl;
                                ++g_audio_stub_state.pcmTraceCount;
                            }
                            blockTransfer.lastPcmNonzeroBytes = nonzero;
                        });
                    }
                    returnValue = runtime->audioBackend().blockTransferStatus(core);
                    blockTransfer.offset = ((returnValue & kAudioPositionMask) - blockTransfer.base) & kAudioPositionMask;
                }
                if (blockTransfer.statusTraceCount < 32u)
                {
                    PS2_IF_AGRESSIVE_LOGS({
                        std::cerr << "[Audio:BlockStatus] core=" << core
                                  << " status=0x" << std::hex << currentBlockStatus(blockTransfer)
                                  << " offset=0x" << blockTransfer.offset
                                  << " size=0x" << blockTransfer.size
                                  << std::dec << std::endl;
                    });
                    ++blockTransfer.statusTraceCount;
                }
            }
            returnValue = runtime ? runtime->audioBackend().blockTransferStatus(core) : currentBlockStatus(blockTransfer);
        }
        else if (cmd == kLibSdCmdSetParam)
        {
            if (runtime)
                runtime->audioBackend().setSpu2Param(static_cast<uint16_t>(cmdArg0), static_cast<uint16_t>(cmdArg1));
            if (g_audio_stub_state.paramTraceCount < 64u)
            {
                PS2_IF_AGRESSIVE_LOGS({
                    std::cerr << "[Audio:SetParam] entry=0x" << std::hex << cmdArg0
                              << " value=0x" << cmdArg1 << std::dec << std::endl;
                });
                ++g_audio_stub_state.paramTraceCount;
            }
            returnValue = 0u;
        }
        else if (cmd == 0x8020u && runtime)
        {
            returnValue = runtime->audioBackend().getSpu2Param(static_cast<uint16_t>(cmdArg0));
        }

        setReturnU32(ctx, returnValue);
    }

    void sceSdRemoteInit(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        (void)rdram;
        if (runtime)
        {
            runtime->audioBackend().stopBlockTransfer(0u);
            runtime->audioBackend().stopBlockTransfer(1u);
        }

        std::lock_guard<std::mutex> lock(g_audio_stub_mutex);
        resetAudioStubStateUnlocked();
        g_audio_stub_state.initialized = true;
        setReturnS32(ctx, 0);
    }

    void sceSdTransToIOP(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t source = getRegU32(ctx, 4) & PS2_RAM_MASK;
        const uint32_t destination = getRegU32(ctx, 5);
        const uint32_t size = getRegU32(ctx, 6);
        const uint32_t blocking = getRegU32(ctx, 7);
        const bool sourceValid = rdram && size <= PS2_RAM_SIZE - source;
        const bool copied = runtime && sourceValid && blocking != 0u &&
                            runtime->writeIopMemory(destination, rdram + source, size);
        bool trace = false;
        {
            std::lock_guard<std::mutex> lock(g_audio_stub_mutex);
            trace = g_audio_stub_state.transToIopTraceCount < 16u;
            if (trace)
                ++g_audio_stub_state.transToIopTraceCount;
        }
        if (trace)
        {
            PS2_IF_AGRESSIVE_LOGS({
                uint32_t nonzeroSourceBytes = 0u;
                if (sourceValid)
                {
                    for (uint32_t index = 0u; index < size; ++index)
                        nonzeroSourceBytes += rdram[source + index] != 0u;
                }
                std::cerr << "[Audio:TransToIOP] src=0x" << std::hex << source
                          << " dst=0x" << destination << " size=0x" << size
                          << " blocking=" << std::dec << blocking
                          << " copied=" << copied
                          << " nonzeroSourceBytes=" << nonzeroSourceBytes << std::endl;
            });
        }
        if (!copied)
        {
            setReturnS32(ctx, -1);
            return;
        }
        setReturnS32(ctx, 0);
    }

    void sceSSyn_BreakAtick(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_BreakAtick", rdram, ctx, runtime);
    }

    void sceSSyn_ClearBreakAtick(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_ClearBreakAtick", rdram, ctx, runtime);
    }

    void sceSSyn_SendExcMsg(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_SendExcMsg", rdram, ctx, runtime);
    }

    void sceSSyn_SendNrpnMsg(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_SendNrpnMsg", rdram, ctx, runtime);
    }

    void sceSSyn_SendRpnMsg(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_SendRpnMsg", rdram, ctx, runtime);
    }

    void sceSSyn_SendShortMsg(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_SendShortMsg", rdram, ctx, runtime);
    }

    void sceSSyn_SetChPriority(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_SetChPriority", rdram, ctx, runtime);
    }

    void sceSSyn_SetMasterVolume(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_SetMasterVolume", rdram, ctx, runtime);
    }

    void sceSSyn_SetOutPortVolume(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_SetOutPortVolume", rdram, ctx, runtime);
    }

    void sceSSyn_SetOutputAssign(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_SetOutputAssign", rdram, ctx, runtime);
    }

    void sceSSyn_SetOutputMode(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        setReturnS32(ctx, 0);
    }

    void sceSSyn_SetPortMaxPoly(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_SetPortMaxPoly", rdram, ctx, runtime);
    }

    void sceSSyn_SetPortVolume(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_SetPortVolume", rdram, ctx, runtime);
    }

    void sceSSyn_SetTvaEnvMode(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSSyn_SetTvaEnvMode", rdram, ctx, runtime);
    }

    void sceSynthesizerAmpProcI(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerAmpProcI", rdram, ctx, runtime);
    }

    void sceSynthesizerAmpProcNI(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerAmpProcNI", rdram, ctx, runtime);
    }

    void sceSynthesizerAssignAllNoteOff(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerAssignAllNoteOff", rdram, ctx, runtime);
    }

    void sceSynthesizerAssignAllSoundOff(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerAssignAllSoundOff", rdram, ctx, runtime);
    }

    void sceSynthesizerAssignHoldChange(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerAssignHoldChange", rdram, ctx, runtime);
    }

    void sceSynthesizerAssignNoteOff(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerAssignNoteOff", rdram, ctx, runtime);
    }

    void sceSynthesizerAssignNoteOn(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerAssignNoteOn", rdram, ctx, runtime);
    }

    void sceSynthesizerCalcEnv(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerCalcEnv", rdram, ctx, runtime);
    }

    void sceSynthesizerCalcPortamentPitch(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerCalcPortamentPitch", rdram, ctx, runtime);
    }

    void sceSynthesizerCalcTvfCoefAll(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerCalcTvfCoefAll", rdram, ctx, runtime);
    }

    void sceSynthesizerCalcTvfCoefF0(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerCalcTvfCoefF0", rdram, ctx, runtime);
    }

    void sceSynthesizerCent2PhaseInc(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerCent2PhaseInc", rdram, ctx, runtime);
    }

    void sceSynthesizerChangeEffectSend(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangeEffectSend", rdram, ctx, runtime);
    }

    void sceSynthesizerChangeHsPanpot(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangeHsPanpot", rdram, ctx, runtime);
    }

    void sceSynthesizerChangeNrpnCutOff(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangeNrpnCutOff", rdram, ctx, runtime);
    }

    void sceSynthesizerChangeNrpnLfoDepth(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangeNrpnLfoDepth", rdram, ctx, runtime);
    }

    void sceSynthesizerChangeNrpnLfoRate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangeNrpnLfoRate", rdram, ctx, runtime);
    }

    void sceSynthesizerChangeOutAttrib(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangeOutAttrib", rdram, ctx, runtime);
    }

    void sceSynthesizerChangeOutVol(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangeOutVol", rdram, ctx, runtime);
    }

    void sceSynthesizerChangePanpot(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangePanpot", rdram, ctx, runtime);
    }

    void sceSynthesizerChangePartBendSens(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangePartBendSens", rdram, ctx, runtime);
    }

    void sceSynthesizerChangePartExpression(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangePartExpression", rdram, ctx, runtime);
    }

    void sceSynthesizerChangePartHsExpression(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangePartHsExpression", rdram, ctx, runtime);
    }

    void sceSynthesizerChangePartHsPitchBend(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangePartHsPitchBend", rdram, ctx, runtime);
    }

    void sceSynthesizerChangePartModuration(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangePartModuration", rdram, ctx, runtime);
    }

    void sceSynthesizerChangePartPitchBend(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangePartPitchBend", rdram, ctx, runtime);
    }

    void sceSynthesizerChangePartVolume(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangePartVolume", rdram, ctx, runtime);
    }

    void sceSynthesizerChangePortamento(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangePortamento", rdram, ctx, runtime);
    }

    void sceSynthesizerChangePortamentoTime(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerChangePortamentoTime", rdram, ctx, runtime);
    }

    void sceSynthesizerClearKeyMap(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerClearKeyMap", rdram, ctx, runtime);
    }

    void sceSynthesizerClearSpr(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerClearSpr", rdram, ctx, runtime);
    }

    void sceSynthesizerCopyOutput(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerCopyOutput", rdram, ctx, runtime);
    }

    void sceSynthesizerDmaFromSPR(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerDmaFromSPR", rdram, ctx, runtime);
    }

    void sceSynthesizerDmaSpr(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerDmaSpr", rdram, ctx, runtime);
    }

    void sceSynthesizerDmaToSPR(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerDmaToSPR", rdram, ctx, runtime);
    }

    void sceSynthesizerGetPartial(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerGetPartial", rdram, ctx, runtime);
    }

    void sceSynthesizerGetPartOutLevel(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerGetPartOutLevel", rdram, ctx, runtime);
    }

    void sceSynthesizerGetSampleParam(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerGetSampleParam", rdram, ctx, runtime);
    }

    void sceSynthesizerHsMessage(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerHsMessage", rdram, ctx, runtime);
    }

    void sceSynthesizerLfoNone(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerLfoNone", rdram, ctx, runtime);
    }

    void sceSynthesizerLfoProc(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerLfoProc", rdram, ctx, runtime);
    }

    void sceSynthesizerLfoSawDown(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerLfoSawDown", rdram, ctx, runtime);
    }

    void sceSynthesizerLfoSawUp(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerLfoSawUp", rdram, ctx, runtime);
    }

    void sceSynthesizerLfoSquare(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerLfoSquare", rdram, ctx, runtime);
    }

    void sceSynthesizerReadNoise(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerReadNoise", rdram, ctx, runtime);
    }

    void sceSynthesizerReadNoiseAdd(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerReadNoiseAdd", rdram, ctx, runtime);
    }

    void sceSynthesizerReadSample16(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerReadSample16", rdram, ctx, runtime);
    }

    void sceSynthesizerReadSample16Add(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerReadSample16Add", rdram, ctx, runtime);
    }

    void sceSynthesizerReadSample8(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerReadSample8", rdram, ctx, runtime);
    }

    void sceSynthesizerReadSample8Add(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerReadSample8Add", rdram, ctx, runtime);
    }

    void sceSynthesizerResetPart(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerResetPart", rdram, ctx, runtime);
    }

    void sceSynthesizerRestorDma(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerRestorDma", rdram, ctx, runtime);
    }

    void sceSynthesizerSelectPatch(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSelectPatch", rdram, ctx, runtime);
    }

    void sceSynthesizerSendShortMessage(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSendShortMessage", rdram, ctx, runtime);
    }

    void sceSynthesizerSetMasterVolume(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSetMasterVolume", rdram, ctx, runtime);
    }

    void sceSynthesizerSetRVoice(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSetRVoice", rdram, ctx, runtime);
    }

    void sceSynthesizerSetupDma(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSetupDma", rdram, ctx, runtime);
    }

    void sceSynthesizerSetupLfo(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSetupLfo", rdram, ctx, runtime);
    }

    void sceSynthesizerSetupMidiModuration(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSetupMidiModuration", rdram, ctx, runtime);
    }

    void sceSynthesizerSetupMidiPanpot(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSetupMidiPanpot", rdram, ctx, runtime);
    }

    void sceSynthesizerSetupNewNoise(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSetupNewNoise", rdram, ctx, runtime);
    }

    void sceSynthesizerSetupReleaseEnv(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSetupReleaseEnv", rdram, ctx, runtime);
    }

    void sceSynthesizerSetuptEnv(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSetuptEnv", rdram, ctx, runtime);
    }

    void sceSynthesizerSetupTruncateTvaEnv(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSetupTruncateTvaEnv", rdram, ctx, runtime);
    }

    void sceSynthesizerSetupTruncateTvfPitchEnv(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerSetupTruncateTvfPitchEnv", rdram, ctx, runtime);
    }

    void sceSynthesizerTonegenerator(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerTonegenerator", rdram, ctx, runtime);
    }

    void sceSynthesizerTransposeMatrix(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerTransposeMatrix", rdram, ctx, runtime);
    }

    void sceSynthesizerTvfProcI(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerTvfProcI", rdram, ctx, runtime);
    }

    void sceSynthesizerTvfProcNI(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerTvfProcNI", rdram, ctx, runtime);
    }

    void sceSynthesizerWaitDmaFromSPR(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerWaitDmaFromSPR", rdram, ctx, runtime);
    }

    void sceSynthesizerWaitDmaToSPR(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthesizerWaitDmaToSPR", rdram, ctx, runtime);
    }

    void sceSynthsizerGetDrumPatch(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthsizerGetDrumPatch", rdram, ctx, runtime);
    }

    void sceSynthsizerGetMeloPatch(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthsizerGetMeloPatch", rdram, ctx, runtime);
    }

    void sceSynthsizerLfoNoise(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthsizerLfoNoise", rdram, ctx, runtime);
    }

    void sceSynthSizerLfoTriangle(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        TODO_NAMED("sceSynthSizerLfoTriangle", rdram, ctx, runtime);
    }
}
