#pragma once
#include "runtime/gs/gs_backend.h"
#include "runtime/gs/gs_parallel_device.h"
#include <memory>

// Raw GS registers/GIF are rendered by paraLLEl-GS. The existing CPU device is
// used for presentation/readback and selected host memory operations only.
class GSParallelBackend final : public GSRasterBackend
{
public:
    GSParallelBackend();
    ~GSParallelBackend() override;
    void Initialize(uint8_t *, uint32_t) override;
    void Reset() override;
    void Submit(const GSPrimitiveBatch &) override;
    void LoadClut(const GSTex0Reg &, const GSTexClutReg &) override;
    void BeginTransfer(const GSTransferCommand &) override;
    void UploadImage(const uint8_t *, uint32_t) override;
    void Flush() override;
    void TextureFlush() override;
    void Sync(GSSyncReason) override;
    PresentationFrame Present(const GSPresentationRequest &) override;
    bool ClearFramebuffer(const GSContext &, uint32_t) override;
    uint32_t ConsumeLocalToHostBytes(uint8_t *, uint32_t) override;
    uint32_t ReadVram(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) const override;
    void WriteVram(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) override;
    void SnapshotVram(std::vector<uint8_t> &) const override;
    GSTransferSnapshot GetTransferSnapshot() const override;
    GSGifStreamState *GifStreamState(uint32_t) override;
    bool ConsumesRawRegisters() const override { return true; }
    void SubmitGifStream(const uint8_t *, uint32_t, uint32_t) override;
    void WriteRegister(uint8_t, uint64_t) override;
    void ObserveImageData(const uint8_t *, uint32_t) override;
    ps2native::gs::ParallelDeviceStats stats() const;
    std::string deviceName() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
