#include "nexo/vu_native.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

using ps2native::nexo::VuNativeAccess;
namespace
{
void usage(const VuNativeAccess::InstructionUsage &u)
{
    std::cout << "{\"vfRead\":[";
    for (unsigned i = 0; i < 2; ++i)
    {
        if (i) std::cout << ',';
        std::cout << '[' << unsigned(u.vfRead[i].reg) << ',' << unsigned(u.vfRead[i].lanes) << ']';
    }
    std::cout << "],\"vfWrite\":[" << unsigned(u.vfWrite.reg) << ',' << unsigned(u.vfWrite.lanes) << ']';
#define FIELD(name) std::cout << ",\"" #name "\":" << unsigned(u.name)
    FIELD(vfReadCount); FIELD(viRead); FIELD(viWrite); FIELD(accRead); FIELD(accWrite);
    FIELD(latency); FIELD(vfLatency); FIELD(viLatency); FIELD(pipeline);
    FIELD(waitQ); FIELD(waitP); FIELD(readsClip); FIELD(writesClip); FIELD(delaysNextBranchRead); FIELD(reserved);
#undef FIELD
    std::cout << '}';
}
}

int main(int argc, char **argv)
{
    try
    {
        if (argc != 3 || (std::string_view(argv[2]) != "vu0" && std::string_view(argv[2]) != "vu1"))
            throw std::invalid_argument("usage: nexo_vu_inspect <microcode-memory-file> <vu0|vu1>");
        const auto unit = std::string_view(argv[2]) == "vu1" ? VuNativeAccess::Unit::VU1 : VuNativeAccess::Unit::VU0;
        const size_t expected = unit == VuNativeAccess::Unit::VU1 ? 16384 : 4096;
        std::vector<uint8_t> code(expected);
        std::ifstream stream(argv[1], std::ios::binary);
        stream.read(reinterpret_cast<char *>(code.data()), static_cast<std::streamsize>(code.size()));
        if (!stream || stream.peek() != std::char_traits<char>::eof())
            throw std::invalid_argument("cannot read exact microcode memory");
        const auto pairs = VuNativeAccess::inspectMicrocode(code, unit);
        std::cout << "{\"version\":1,\"unit\":" << unsigned(unit) << ",\"code_size\":" << expected << ",\"pairs\":[";
        for (size_t i = 0; i < pairs.size(); ++i)
        {
            if (i) std::cout << ',';
            const auto &p = pairs[i];
            std::cout << "{\"lower\":" << p.lower << ",\"upper\":" << p.upper << ",\"lowerUsage\":";
            usage(p.lowerUsage); std::cout << ",\"upperUsage\":"; usage(p.upperUsage);
#define FIELD(name) std::cout << ",\"" #name "\":" << unsigned(p.name)
            FIELD(iBit); FIELD(eBit); FIELD(mBit); FIELD(dBit); FIELD(tBit);
            FIELD(upperVfShadowReg); FIELD(suppressedLowerVf);
#undef FIELD
            std::cout << '}';
        }
        std::cout << "]}\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "[nexo-vu-inspect:error] " << error.what() << '\n';
        return 1;
    }
}
