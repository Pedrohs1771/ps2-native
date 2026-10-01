#include "nexo/ee_snapshot.h"
#include "nexo/canonical_binary.h"

namespace ps2native::nexo
{
    namespace
    {
        constexpr std::array<uint8_t,8> magic{'N','E','X','O','E','E',0,0};
        constexpr size_t maximumBytes=8192;
        constexpr uint32_t modelProfile=1;

        template<typename Archive,typename Value> void integerVector(Archive &a,Value &value)
        {
            std::array<uint32_t,4> lanes{};
            if constexpr (!Archive::reading)
                _mm_storeu_si128(reinterpret_cast<__m128i*>(lanes.data()),value);
            a(lanes);
            if constexpr (Archive::reading)
                value=_mm_set_epi32(std::bit_cast<int32_t>(lanes[3]),std::bit_cast<int32_t>(lanes[2]),
                                    std::bit_cast<int32_t>(lanes[1]),std::bit_cast<int32_t>(lanes[0]));
        }
        template<typename Archive,typename Value> void floatVector(Archive &a,Value &value)
        {
            __m128i bits{};
            if constexpr (!Archive::reading) bits=_mm_castps_si128(value);
            integerVector(a,bits);
            if constexpr (Archive::reading) value=_mm_castsi128_ps(bits);
        }
        template<typename Archive,typename Context> void fields(Archive &a,Context &c)
        {
            for(auto &value:c.r) integerVector(a,value);
            a(c.pc);a(c.insn_count);a(c.hi);a(c.lo);a(c.hi1);a(c.lo1);a(c.sa);
            for(auto &value:c.vu0_vf) floatVector(a,value);
            a(c.vi);a(c.vu0_q);a(c.vu0_p);a(c.vu0_i);
            floatVector(a,c.vu0_r);floatVector(a,c.vu0_acc);
            a(c.vu0_status);a(c.vu0_mac_flags);a(c.vu0_clip_flags);a(c.vu0_clip_flags2);
            a(c.vu0_cmsar0);a(c.vu0_cmsar1);a(c.vu0_cmsar2);a(c.vu0_cmsar3);
            a(c.vu0_vpu_stat);a(c.vu0_vpu_stat2);a(c.vu0_vpu_stat3);a(c.vu0_vpu_stat4);
            a(c.vu0_tpc);a(c.vu0_tpc2);
            a(c.vu0_fbrst);a(c.vu0_fbrst2);a(c.vu0_fbrst3);a(c.vu0_fbrst4);
            a(c.vu0_itop);a(c.vu0_top);a(c.vu0_info);a(c.vu0_xitop);a(c.vu0_pc);a(c.vu0_cf);
            a(c.cop0_index);a(c.cop0_random);a(c.cop0_entrylo0);a(c.cop0_entrylo1);
            a(c.cop0_context);a(c.cop0_pagemask);a(c.cop0_wired);a(c.cop0_badvaddr);
            a(c.cop0_count);a(c.cop0_entryhi);a(c.cop0_compare);a(c.cop0_status);
            a(c.cop0_cause);a(c.cop0_epc);a(c.cop0_prid);a(c.cop0_config);
            a(c.cop0_badpaddr);a(c.cop0_debug);a(c.cop0_perf);a(c.cop0_taglo);a(c.cop0_taghi);a(c.cop0_errorepc);
            a(c.llbit);a(c.lladdr);a(c.in_delay_slot);a(c.branch_pc);a(c.cop2_ccr);
            a(c.f);a(c.f_acc);a(c.fcr31);
        }
    }

    std::vector<uint8_t> EeSnapshotCodec::encode(const R5900Context &context)
    {
        binary::Writer writer(magic,modelProfile,maximumBytes);
        fields(writer,context);
        return writer.finish();
    }
    R5900Context EeSnapshotCodec::decode(std::span<const uint8_t> bytes)
    {
        binary::Reader reader(bytes,magic,modelProfile,maximumBytes);
        R5900Context context{};
        fields(reader,context);reader.finish();return context;
    }
}
