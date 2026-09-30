#include "nexo/device_snapshot.h"
#include "nexo/canonical_binary.h"
#include "runtime/ps2_memory.h"
#include "runtime/gs/gs_frontend.h"
#include "runtime/gs/gs_cpu_backend.h"

#include <concepts>
#include <mutex>

namespace ps2native::nexo::binary
{
// Explicit fields also preserve the bits of non-finite floating point values.
// The const-qualified instantiations are used only by Writer.
#define GS_FIELDS(Type, Body) \
    template <typename A, typename T> requires std::same_as<std::remove_cv_t<T>, Type> \
    void canonicalFields(A &a, T &v) { Body }
GS_FIELDS(GSVertex, a(v.x); a(v.y); a(v.z); a(v.r); a(v.g); a(v.b); a(v.a);
    a(v.q); a(v.s); a(v.t); a(v.u); a(v.v); a(v.fog);)
GS_FIELDS(GSFrameReg, a(v.fbp); a(v.fbw); a(v.psm); a(v.fbmsk);)
GS_FIELDS(GSZbufReg, a(v.zbp); a(v.psm); a(v.zmask);)
GS_FIELDS(GSScissorReg, a(v.x0); a(v.x1); a(v.y0); a(v.y1);)
GS_FIELDS(GSTex0Reg, a(v.tbp0); a(v.tbw); a(v.psm); a(v.tw); a(v.th); a(v.tcc); a(v.tfx);
    a(v.cbp); a(v.cpsm); a(v.csm); a(v.csa); a(v.cld);)
GS_FIELDS(GSXYOffsetReg, a(v.ofx); a(v.ofy);)
GS_FIELDS(GSTexaReg, a(v.ta0); a(v.aem); a(v.ta1);)
GS_FIELDS(GSTexClutReg, a(v.cbw); a(v.cou); a(v.cov);)
GS_FIELDS(GSContext, a(v.frame); a(v.scissor); a(v.tex0); a(v.xyoffset); a(v.zbuf);
    a(v.tex1); a(v.miptbp1); a(v.miptbp2); a(v.clamp); a(v.alpha); a(v.test); a(v.fba);)
GS_FIELDS(GSPrimReg, a(v.type); a(v.iip); a(v.tme); a(v.fge); a(v.abe); a(v.aa1); a(v.fst); a(v.ctxt); a(v.fix);)
GS_FIELDS(GSBitBltBuf, a(v.sbp); a(v.sbw); a(v.spsm); a(v.dbp); a(v.dbw); a(v.dpsm);)
GS_FIELDS(GSTrxPos, a(v.ssax); a(v.ssay); a(v.dsax); a(v.dsay); a(v.dir);)
GS_FIELDS(GSTrxReg, a(v.rrw); a(v.rrh);)
GS_FIELDS(GSTransferCommand, a(v.bitbltbuf); a(v.trxpos); a(v.trxreg); a(v.direction);)
#undef GS_FIELDS
}

namespace ps2native::nexo
{
namespace
{
constexpr std::array<uint8_t, 8> gsMagic{'N','E','X','O','G','S',0,0};
constexpr size_t gsBound = 128u * 1024u * 1024u;
static_assert(sizeof(int) == sizeof(int32_t));

template <typename T> void copyField(T &dst, const T &src) { dst = src; }
template <typename T, size_t N> void copyField(T (&dst)[N], const T (&src)[N])
{ std::copy(std::begin(src), std::end(src), std::begin(dst)); }
template <typename T> void publishField(T &dst, T &src) noexcept { dst = src; }
template <typename T, size_t N> void publishField(T (&dst)[N], T (&src)[N]) noexcept
{ std::copy(std::begin(src), std::end(src), std::begin(dst)); }
void publishField(std::vector<uint8_t> &dst, std::vector<uint8_t> &src) noexcept { dst.swap(src); }

// A single field list keeps capture, canonical order, and publication in sync.
#define GS_FRONT_FIELDS(F) \
    F(m_ctx) F(m_prim) F(m_primRegister) F(m_prmodeRegister) \
    F(m_curR) F(m_curG) F(m_curB) F(m_curA) F(m_curQ) F(m_curS) F(m_curT) F(m_curU) F(m_curV) \
    F(m_curFog) F(m_fogR) F(m_fogG) F(m_fogB) F(m_prmodecont) F(m_pabe) \
    F(m_scanmsk) F(m_dimx) F(m_dthe) F(m_colclamp) F(m_texa) F(m_texclut) \
    F(m_bitbltbuf) F(m_trxpos) F(m_trxreg) F(m_trxdir) F(m_vtxQueue) F(m_vtxCount) F(m_vtxIndex) \
    F(m_displaySnapshot) F(m_lastDisplayBaseBytes) F(m_preferredDisplaySourceFrame) \
    F(m_preferredDisplayDestFbp) F(m_hasPreferredDisplaySource) \
    F(m_hostPresentationFrame) F(m_hostPresentationWidth) F(m_hostPresentationHeight) \
    F(m_hostPresentationDisplayFbp) F(m_hostPresentationSourceFbp) F(m_hostPresentationUsedPreferred) \
    F(m_hasHostPresentationFrame) F(m_nativeImageUploadCount) F(m_nativePackedGIFPacketCount)
#define GS_PRIV_PLAIN(F) \
    F(pmode,0) F(smode1,1) F(smode2,2) F(srfsh,3) F(synch1,4) F(synch2,5) F(syncv,6) \
    F(dispfb1,7) F(display1,8) F(dispfb2,9) F(display2,10) F(extbuf,11) F(extdata,12) F(extwrite,13) \
    F(bgcolor,14) F(imr,17) F(busdir,18) F(siglblid,19)
}

struct GsSnapshotCodec::State
{
#define DECLARE(name) decltype(GS::name) name{};
    GS_FRONT_FIELDS(DECLARE)
#undef DECLARE
    bool hasPriv = false;
    std::array<uint64_t, 20> priv{};
    std::vector<uint8_t> vram;
    std::array<uint16_t, 512> clut{};
    std::array<uint32_t, 2> clutCbp{};
    uint32_t cachePageBase = UINT32_MAX;
    std::array<uint8_t, GSMem::TexturePageCache::kPageSize> cacheBytes{};
    GSTransferCommand transfer{};
    GSTransferSnapshot transferState{};
    std::vector<uint8_t> localToHostBuffer;
    uint64_t localToHostReadPos = 0;
};

template <typename A, typename G> void GsSnapshotCodec::frontendFields(A &a, G &g)
{
#define FIELD(name) a(g.name);
    GS_FRONT_FIELDS(FIELD)
#undef FIELD
}
template <typename A> void GsSnapshotCodec::stateFields(A &a, State &s)
{
    a(s.hasPriv); a(s.priv); a(s.vram); frontendFields(a, s);
    a(s.clut); a(s.clutCbp); a(s.cachePageBase); a(s.cacheBytes); a(s.transfer);
    auto &t = s.transferState;
    a(t.x); a(t.y); a(t.totalPixels); a(t.copiedPixels); a(t.direction);
    uint64_t pending = t.localToHostPendingBytes; a(pending);
    if constexpr (A::reading)
    {
        if (pending > SIZE_MAX) throw std::invalid_argument("GS pending byte count exceeds host size");
        t.localToHostPendingBytes = size_t(pending);
    }
    a(s.localToHostBuffer); a(s.localToHostReadPos);
}
void GsSnapshotCodec::validate(const State &s)
{
    const auto validPrim = [](const GSPrimReg &p) { return static_cast<uint8_t>(p.type) <= 7; };
    if (s.vram.size() != PS2_GS_VRAM_SIZE || s.m_trxdir > 3 || s.transfer.direction > 3 ||
        s.transferState.direction > 3 || s.m_vtxCount < 0 || s.m_vtxIndex < 0 ||
        !validPrim(s.m_prim) || !validPrim(s.m_primRegister) || !validPrim(s.m_prmodeRegister) ||
        s.localToHostReadPos > s.localToHostBuffer.size() ||
        s.transferState.localToHostPendingBytes > s.localToHostBuffer.size() ||
        (!s.m_displaySnapshot.empty() && s.m_displaySnapshot.size() != PS2_GS_VRAM_SIZE) ||
        (s.cachePageBase != UINT32_MAX &&
         (s.cachePageBase > PS2_GS_VRAM_SIZE - GSMem::TexturePageCache::kPageSize ||
          s.cachePageBase % GSMem::TexturePageCache::kPageSize != 0)))
        throw std::invalid_argument("invalid GS CPU checkpoint extent or cursor");
    if (s.transferState.totalPixels != 0 && s.transferState.copiedPixels > s.transferState.totalPixels)
        throw std::invalid_argument("invalid GS transfer progress");
    if (s.m_hasHostPresentationFrame && (s.m_hostPresentationWidth == 0 || s.m_hostPresentationHeight == 0 ||
        uint64_t(s.m_hostPresentationWidth) * s.m_hostPresentationHeight > s.m_hostPresentationFrame.size() / 4))
        throw std::invalid_argument("invalid GS presentation frame extent");
    if (!s.hasPriv && std::any_of(s.priv.begin(), s.priv.end(), [](uint64_t v) { return v != 0; }))
        throw std::invalid_argument("unbound GS privileged register checkpoint must be zero");
}

std::vector<uint8_t> GsSnapshotCodec::encode(const GS &gs)
{
    State s;
    {
        std::scoped_lock lock(gs.m_stateMutex, gs.m_backendLifetimeMutex, gs.m_presentationMutex, gs.m_snapshotMutex);
        const auto *cpu = dynamic_cast<const GSCpuBackend *>(gs.m_backend.get());
        if (!cpu) throw std::invalid_argument("GS checkpoint requires the identified CPU backend");
        std::lock_guard cpuLock(cpu->m_mutex);
        if (!gs.m_localMemoryStorage || gs.m_localMemorySize != PS2_GS_VRAM_SIZE ||
            cpu->m_vram != gs.m_localMemoryStorage || cpu->m_vramSize != gs.m_localMemorySize)
            throw std::invalid_argument("GS checkpoint requires initialized coherent 4 MiB VRAM bindings");
#define COPY(name) copyField(s.name, gs.name);
        GS_FRONT_FIELDS(COPY)
#undef COPY
        s.vram.assign(gs.m_localMemoryStorage, gs.m_localMemoryStorage + gs.m_localMemorySize);
        s.hasPriv = gs.m_privRegs != nullptr;
        if (s.hasPriv)
        {
            const auto &r = *gs.m_privRegs;
#define CAPTURE(name,index) s.priv[index] = r.name;
            GS_PRIV_PLAIN(CAPTURE)
#undef CAPTURE
            s.priv[15] = r.csr.load(std::memory_order_relaxed);
            s.priv[16] = r.vsyncTick.load(std::memory_order_relaxed);
        }
        s.clut = cpu->m_clut; s.clutCbp = cpu->m_clutCbp;
        // A stale page can affect a future texture read after raw VRAM writes.
        // Preserve it as model state instead of silently invalidating it.
        s.cachePageBase = cpu->m_texturePageCache.m_pageBase;
        s.cacheBytes = cpu->m_texturePageCache.m_bytes;
        s.transfer = cpu->m_transfer; s.transferState = cpu->m_transferState;
        s.localToHostBuffer = cpu->m_localToHostBuffer; s.localToHostReadPos = cpu->m_localToHostReadPos;
    }
    validate(s); binary::Writer a(gsMagic, 0, gsBound); stateFields(a, s); return a.finish();
}
void GsSnapshotCodec::restore(GS &gs, std::span<const uint8_t> bytes)
{
    binary::Reader a(bytes, gsMagic, 0, gsBound); State s; stateFields(a, s); a.finish(); validate(s);
    std::scoped_lock lock(gs.m_stateMutex, gs.m_backendLifetimeMutex, gs.m_presentationMutex, gs.m_snapshotMutex);
    auto *cpu = dynamic_cast<GSCpuBackend *>(gs.m_backend.get());
    if (!cpu) throw std::invalid_argument("GS checkpoint requires the identified CPU backend");
    std::lock_guard cpuLock(cpu->m_mutex);
    if (!gs.m_localMemoryStorage || gs.m_localMemorySize != s.vram.size() ||
        cpu->m_vram != gs.m_localMemoryStorage || cpu->m_vramSize != gs.m_localMemorySize ||
        (gs.m_privRegs != nullptr) != s.hasPriv)
        throw std::invalid_argument("GS checkpoint target bindings do not match");
    // All parsing, allocations and rejecting conditions precede publication.
    // Keep target pointers, handlers, callbacks and backend ownership intact.
    std::memcpy(gs.m_localMemoryStorage, s.vram.data(), s.vram.size());
#define PUBLISH(name) publishField(gs.name, s.name);
    GS_FRONT_FIELDS(PUBLISH)
#undef PUBLISH
    if (s.hasPriv)
    {
        auto &r = *gs.m_privRegs;
#define RESTORE(name,index) r.name = s.priv[index];
        GS_PRIV_PLAIN(RESTORE)
#undef RESTORE
        r.csr.store(s.priv[15], std::memory_order_relaxed);
        r.vsyncTick.store(s.priv[16], std::memory_order_relaxed);
    }
    cpu->m_clut = s.clut; cpu->m_clutCbp = s.clutCbp;
    cpu->m_texturePageCache.m_pageBase = s.cachePageBase;
    cpu->m_texturePageCache.m_bytes = s.cacheBytes;
    cpu->m_transfer = s.transfer; cpu->m_transferState = s.transferState;
    cpu->m_localToHostBuffer.swap(s.localToHostBuffer); cpu->m_localToHostReadPos = size_t(s.localToHostReadPos);
}
#undef GS_FRONT_FIELDS
#undef GS_PRIV_PLAIN
}
