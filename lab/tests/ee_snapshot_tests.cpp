#include "MiniTest.h"
#include "nexo/ee_snapshot.h"
#include "nexo/canonical_binary.h"

#include <bit>
#include <cstddef>
#include <cstring>
#include <stdexcept>

using ps2native::nexo::EeSnapshotCodec;

int main()
{
    MiniTest::Case("Canonical EE model context", [](TestCase &suite)
    {
        suite.Run("every_declared_model_field_and_array_cell_survives_round_trip", [](TestCase &test)
        {
            const auto baseline=EeSnapshotCodec::encode(R5900Context{});
            const auto check=[&](const char *name,auto mutate,auto equalField)
            {
                R5900Context context{};mutate(context);
                const auto bytes=EeSnapshotCodec::encode(context);
                test.IsTrue(bytes!=baseline,std::string("Field serialized: ")+name);
                const auto restored=EeSnapshotCodec::decode(bytes);
                test.IsTrue(equalField(context,restored),std::string("Original field bits restored: ")+name);
                test.IsTrue(EeSnapshotCodec::encode(EeSnapshotCodec::decode(bytes))==bytes,
                            std::string("Field restored: ")+name);
            };
#include "ee_context_field_inventory.inc"
        });
        suite.Run("fixed_width_fields_preserve_lane_order_and_float_bits", [](TestCase &test)
        {
            R5900Context ctx{};
            ctx.r[2]=_mm_set_epi32(0x44332211,0x88776655,0xCCBBAA99,0xFFEEDDCC);
            ctx.pc=0x12345678;ctx.insn_count=0x1122334455667788ull;
            ctx.hi=0xFEDCBA9876543210ull;ctx.lo1=0x8877665544332211ull;
            ctx.vu0_vf[3]=_mm_castsi128_ps(ctx.r[2]);
            ctx.vi[15]=0xABCD;ctx.vu0_q=std::bit_cast<float>(0x7F801234u);
            ctx.vu0_p=std::bit_cast<float>(0x80000000u);ctx.f[31]=std::bit_cast<float>(0x7FC055AAu);
            ctx.f_acc=std::bit_cast<float>(0x00000001u);ctx.fcr31=0xBEEF;
            ctx.in_delay_slot=true;ctx.branch_pc=0x12345674;ctx.llbit=1;ctx.lladdr=0x1020;
            auto bytes=EeSnapshotCodec::encode(ctx);
            test.Equals(bytes.size(),size_t{1643},"Canonical length has no ABI padding");
            test.Equals(bytes[24+2*16],uint8_t{0xCC},"Lane zero uses little-endian bytes");
            test.Equals(bytes[24+2*16+12],uint8_t{0x11},"Lane three retains its own order");
            const auto restored=EeSnapshotCodec::decode(bytes);
            test.IsTrue(EeSnapshotCodec::encode(restored)==bytes,"Typed round trip is byte exact");
            test.Equals(restored.pc,ctx.pc,"PC preserved");
            test.Equals(restored.insn_count,ctx.insn_count,"64-bit model counter preserved");
            test.Equals(std::bit_cast<uint32_t>(restored.vu0_q),uint32_t{0x7F801234},"Signaling NaN payload preserved");
            test.Equals(std::bit_cast<uint32_t>(restored.vu0_p),uint32_t{0x80000000},"Signed zero preserved");
            test.Equals(std::bit_cast<uint32_t>(restored.f_acc),uint32_t{1},"Denormal bit pattern preserved");
            auto padded=ctx;
            for(size_t offset=offsetof(R5900Context,pc)+sizeof(ctx.pc);offset<offsetof(R5900Context,insn_count);++offset)
                reinterpret_cast<uint8_t*>(&padded)[offset]^=0xFF;
            test.IsTrue(EeSnapshotCodec::encode(padded)==bytes,"Host padding is excluded");
        });
        suite.Run("corrupt_headers_lengths_checksums_and_booleans_are_rejected", [](TestCase &test)
        {
            auto bytes=EeSnapshotCodec::encode(R5900Context{});
            const auto rejects=[](const std::vector<uint8_t>& data)
            {try{(void)EeSnapshotCodec::decode(data);return false;}catch(const std::invalid_argument&){return true;}};
            for(const size_t offset : {size_t{0},size_t{8},size_t{12},size_t{16},size_t{20},size_t{100}})
            {auto corrupt=bytes;corrupt[offset]^=1;test.IsTrue(rejects(corrupt),"Corruption rejected");}
            auto truncated=bytes;truncated.pop_back();test.IsTrue(rejects(truncated),"Truncated context rejected");
            auto extended=bytes;extended.push_back(0);
            ps2native::nexo::binary::put32(extended,16,extended.size()-24);
            ps2native::nexo::binary::put32(extended,20,ps2native::nexo::binary::checksum(extended));
            test.IsTrue(rejects(extended),"Trailing context fields rejected");
            // 1,350 bytes precede the canonical delay-slot boolean.
            auto boolean=bytes;boolean[24+1350]=2;
            ps2native::nexo::binary::put32(boolean,20,ps2native::nexo::binary::checksum(boolean));
            test.IsTrue(rejects(boolean),"Noncanonical boolean rejected with a valid checksum");
        });
    });
    return MiniTest::Run();
}
