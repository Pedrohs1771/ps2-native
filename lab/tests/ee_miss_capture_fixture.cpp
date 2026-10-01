#include "nexo/ee_miss_capture.h"
#include "nexo/ee_snapshot.h"
#include "ps2_ee_aot.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace { void callback(uint8_t *,R5900Context *,PS2Runtime *) {} }
int main(int argc,char **argv)
{
    if(argc!=3) return 2;
    const std::string mode(argv[1]);
    if(mode!="disabled") setenv("PS2X_EE_MISS_CAPTURE_DIR",argv[2],1);
    else unsetenv("PS2X_EE_MISS_CAPTURE_DIR");
    std::vector<uint8_t> ram(PS2_RAM_SIZE);
    const std::array<uint8_t,12> image{42,0,2,36,8,0,224,3,1,0,66,36};
    std::memcpy(ram.data()+0x10000,image.data(),image.size());
    const PS2NativeOverlayBinding bindings[]={{0x10004,callback,0x10000,12,image.data()}};
    const ps2native::ee_aot::Bank bank{0x10000,image,bindings};
    const ps2native::ee_aot::Dispatcher dispatcher({std::span(&bank,1)});
    R5900Context ctx{};ctx.pc=0x10004;ctx.r[4]=_mm_set_epi32(4,3,2,1);
    uint32_t target=0x10004;
    const uint8_t *memory=ram.data();
    if(mode=="changed" || mode=="disabled" || mode=="no-context") ram[0x10000]^=1;
    if(mode=="missing") target=0x1000C;
    if(mode=="no-ram" || mode=="saturate") memory=nullptr;
    const auto beforeRam=ram;
    const auto beforeContext=ps2native::nexo::EeSnapshotCodec::encode(ctx);
    const unsigned count=mode=="saturate" ? 18 : 1;
    for(unsigned index=0;index<count;++index)
        ps2native::nexo::captureEeMiss(memory,mode=="no-context" ? nullptr : &ctx,&dispatcher,target,0x10010,
            PS2Runtime::GuestBranchKind::IndirectCall,"fixture\"\\\n",mode=="shadowed","cdrom0:\\module.elf;1");
    std::cout << "{\"attempts\":" << count << ",\"guest_bytes_unchanged\":"
              << (ram==beforeRam ? "true" : "false")
              << ",\"guest_context_unchanged\":"
              << (ps2native::nexo::EeSnapshotCodec::encode(ctx)==beforeContext ? "true" : "false") << "}\n";
}
