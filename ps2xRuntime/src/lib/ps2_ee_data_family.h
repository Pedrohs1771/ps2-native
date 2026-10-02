#pragma once
#include "ps2_runtime.h"
#include <span>
#include <unordered_map>
#include <vector>

namespace ps2native::ee_family
{
    inline constexpr size_t MaxFamilies=32768;
    using Function = void (*)(uint8_t *,R5900Context *,PS2Runtime *,uint32_t);
    struct Family
    {
        Function function;
        std::span<const uint32_t> words;
        std::span<const uint32_t> masks;
        std::span<const uint32_t> normalOffsets;
    };
    struct Program { std::span<const Family> families; };
    enum class Status { Ready, MissingEntry, Ambiguous, NoRam, MisalignedPc, OutsideRam, UnsupportedEntryContext };
    struct Admission
    {
        Function function=nullptr;
        uint32_t base=0;
        Status status=Status::MissingEntry;
        uint32_t candidatesChecked=0;
    };
    // Identity guards over immutable precompiled structures, not ISA execution.
    // RAM/fetch and producer-domain equivalence remain laboratory obligations.
    class Dispatcher
    {
    public:
        explicit Dispatcher(const Program &program);
        [[nodiscard]] Admission lookup(const uint8_t *ram,uint32_t pc) const;
        [[nodiscard]] Admission admitNormalEntry(const uint8_t *ram,const R5900Context *context) const;
        [[nodiscard]] size_t families() const noexcept { return m_families.size(); }
    private:
        struct Owned { Function function;std::vector<uint32_t> words,masks; };
        struct Entry { uint32_t family,offset; };
        std::vector<Owned> m_families;
        std::unordered_map<uint32_t,std::vector<Entry>> m_exact,m_upper;
    };
    const Dispatcher *compiledDispatcher();
}
