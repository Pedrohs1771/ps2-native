#pragma once

#include "runtime/gs/gs_types.h"

#include <cstdint>
#include <vector>

// State belongs to the backend which consumes raw GIF. Keeping it there leaves
// the GS/runtime object layout intact and preserves independent GIF paths.
struct GSGifStreamState
{
    uint64_t registers = 0;
    uint32_t loopsRemaining = 0;
    uint8_t registerCount = 0;
    uint8_t registerIndex = 0;
    uint8_t format = 0;
};

class GSRasterBackend
{
public:
    virtual ~GSRasterBackend() = default;

    virtual void Initialize(uint8_t *vram, uint32_t vramSize) = 0;
    virtual void Reset() = 0;

    virtual void Submit(const GSPrimitiveBatch &batch) = 0;
    virtual void LoadClut(const GSTex0Reg &tex0, const GSTexClutReg &texclut) = 0;

    virtual void BeginTransfer(const GSTransferCommand &command) = 0;
    virtual void UploadImage(const uint8_t *data, uint32_t sizeBytes) = 0;

    virtual void Flush() = 0;
    virtual void TextureFlush() = 0;
    virtual void Sync(GSSyncReason reason) = 0;
    virtual PresentationFrame Present(const GSPresentationRequest &request) = 0;

    virtual bool ClearFramebuffer(const GSContext &context, uint32_t rgba) = 0;
    virtual uint32_t ConsumeLocalToHostBytes(uint8_t *dst, uint32_t maxBytes) = 0;

    virtual uint32_t ReadVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y) const = 0;
    virtual void WriteVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y, uint32_t value) = 0;
    virtual void SnapshotVram(std::vector<uint8_t> &out) const = 0;
    virtual GSTransferSnapshot GetTransferSnapshot() const = 0;

    // Optional raw transport. CPU backends retain the existing batch contract.
    virtual GSGifStreamState *GifStreamState(uint32_t) { return nullptr; }
    virtual void SubmitGifStream(const uint8_t *, uint32_t, uint32_t) {}
    virtual void WriteRegister(uint8_t, uint64_t) {}
    virtual void ObserveImageData(const uint8_t *, uint32_t) {}
    // Raw backends lower vertex register writes themselves; Submit is the
    // alternative batch API, not an additional copy of the same primitive.
    virtual bool ConsumesRawRegisters() const { return false; }
};
