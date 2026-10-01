#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace ps2native::nexo
{
// Architectural projection only. Upstream hidden pipeline state stays inside
// its engine; this is deliberately not a canonical NEXO hidden-state export.
struct VuReferenceProjection
{
    std::array<uint32_t,128> vf{};
    std::array<uint16_t,16> vi{};
    std::array<uint32_t,4> acc{};
    uint32_t q=0,p=0,i=0,r=0,mac=0,status=0,clip=0,pc=0;
    uint64_t cycles=0;
};
struct VuReferenceChunk
{
    uint64_t cycle=0;
    bool packetEnd=false;
    std::vector<uint8_t> bytes;
};
struct VuReferenceIssue
{
    uint64_t cycle=0; // Upstream clock after its leading tick and issue stalls.
    uint32_t pc=0,lower=0,upper=0;
};
class Pcsx2Vu1Reference
{
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    using PacketReceiver=std::function<void(std::span<const uint8_t>)>;
    // Narrow initial domain: an exact reset-state seed, aligned disjoint VU1
    // memories, synchronous execution, and an idle GIF path. No external IRQ,
    // concurrent VU0, or blocked path is silently stubbed into success.
    Pcsx2Vu1Reference(std::span<const uint8_t> canonicalSeed,
        std::span<uint8_t> micro,std::span<uint8_t> data,PacketReceiver receiver);
    ~Pcsx2Vu1Reference();
    Pcsx2Vu1Reference(const Pcsx2Vu1Reference&)=delete;
    Pcsx2Vu1Reference& operator=(const Pcsx2Vu1Reference&)=delete;
    void execute(bool fresh,uint32_t bytePC,uint32_t top,uint32_t itop,uint32_t budget=65536);
    VuReferenceProjection projection() const;
    const std::vector<VuReferenceChunk>& chunks() const;
    uint64_t cycles() const;
    // Observes the unmodified core's upper-dispatch diagnostic point. Disabled
    // by default; enable only before the first microcall of this machine.
    void enableIssueTrace();
    const std::vector<VuReferenceIssue>& issues() const;
};
}
