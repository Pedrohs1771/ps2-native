#include "nexo/vif_capture.h"
#include "nexo/vu_native.h"
#include "vif_case_internal.h"
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <string_view>

#if defined(NEXO_VIF_SINGLE_BANK)
namespace ps2native::nexo { const VuNativeProgram &compiledVuProgram(); }
#elif defined(NEXO_VIF_COMPILED_BANKS)
namespace ps2native::nexo { std::span<const VuNativeProgram> compiledVifBanks(); }
#endif
namespace
{
// Runtime diagnostics must not corrupt the machine-readable result. Replay
// owns this single-threaded process; preserve the existing diagnostics on
// stderr and restore cout even when execution throws.
struct RuntimeDiagnostics
{
    std::streambuf *previous=std::cout.rdbuf(std::cerr.rdbuf());
    ~RuntimeDiagnostics() { std::cout.rdbuf(previous); }
};
uint32_t iterations(std::string_view value)
{
    uint32_t n=0; auto p=std::from_chars(value.data(),value.data()+value.size(),n);
    if (p.ec!=std::errc{} || p.ptr!=value.data()+value.size() || n<1 || n>1000)
        throw std::invalid_argument("invalid VIF iteration count");
    return n;
}
void comparison(const char *name,const std::vector<uint8_t> &actual,const std::vector<uint8_t> &expected)
{
    size_t first=0,common=std::min(actual.size(),expected.size());
    while (first<common && actual[first]==expected[first]) ++first;
    const bool equal=first==common && actual.size()==expected.size();
    // CRC bytes are derived. Report the first differing semantic byte when
    // equal-sized canonical envelopes have only their payloads changed.
    if (!equal && first>=20 && first<24 && actual.size()==expected.size())
    { first=24; while (first<common && actual[first]==expected[first]) ++first; }
    std::cout << '"' << name << "\":{\"equal\":" << (equal?"true":"false") << ",\"first_byte_difference\":";
    if (equal) std::cout << "null"; else std::cout << first; std::cout << '}';
}
}
int main(int argc,char **argv)
{
    try
    {
        if (argc<2 || argc>3) throw std::invalid_argument("usage: nexo_vif_replay <observed-vif-case> [iterations]");
        const std::filesystem::path directory(argv[1]); const uint32_t count=argc==3?iterations(argv[2]):1;
        unsetenv("PS2X_CAPTURE_SCENE");
        setenv("PS2X_FUNCTION_TRACE","0",1);
        using namespace ps2native::nexo;
        const auto expected=detail::readVifFile(directory/"output-state.nexo",24,detail::vifStateBound);
        const auto expectedEvents=detail::readVifFile(directory/"events.nexo",24,detail::vifEventBound);
        const auto recorded=detail::decodeVifBoundary(expected);
        std::vector<std::vector<uint8_t>> banks;
        for (uint32_t i=0;i<256;++i)
        {
            auto p=directory/("bank-"+std::to_string(i)+".bin"); if (!std::filesystem::exists(p)) break;
            banks.push_back(detail::readVifFile(p,16384,16384));
        }
        if (banks.empty()) throw std::invalid_argument("VIF case has no recorded executed bank");
        uint64_t time=0; uint32_t done=0; bool matches=true; VifReplayResult result;
        {
            RuntimeDiagnostics diagnostics;
            for (;done<count;++done)
            {
#if defined(NEXO_VIF_SINGLE_BANK)
                const std::array compiled{compiledVuProgram()}; result=replayVifCaseNative(directory,compiled);
#elif defined(NEXO_VIF_COMPILED_BANKS)
                result=replayVifCaseNative(directory,compiledVifBanks());
#else
                result=replayVifCase(directory);
#endif
                time+=result.executionNanoseconds;
                if (result.state!=expected || result.events!=expectedEvents || result.codeBanks!=banks)
                { matches=false; ++done; break; }
            }
        }
        const auto actual=detail::decodeVifBoundary(result.state);
        std::cout << "{\"scope\":\"observed_vif_vu1_gif_cpu_gs_model\",\"assurance\":\"tested_only\","
                  << "\"vif_input\":\"observed_function_argument\",\"independent_reference\":false,"
                  << "\"final_package_qualified\":false,\"execution_backend\":\""
#if defined(NEXO_VIF_SINGLE_BANK) || defined(NEXO_VIF_COMPILED_BANKS)
                  << "conservative_aot_vu1_banks"
#else
                  << "current_vu_interpreter"
#endif
                  << "\",\"matches_recording\":" << (matches?"true":"false") << ",\"iterations\":" << done
                  << ",\"mean_execution_us\":" << double(time)/(1000.0*done) << ",\"executed_banks\":" << result.codeBanks.size() << ',';
        comparison("vif",actual.vif,recorded.vif); std::cout << ',';
        comparison("vu",actual.vu,recorded.vu); std::cout << ',';
        comparison("gif",actual.gif,recorded.gif); std::cout << ',';
        comparison("gs",actual.gs,recorded.gs); std::cout << ',';
        comparison("code",actual.code,recorded.code); std::cout << ',';
        comparison("data",actual.data,recorded.data); std::cout << ',';
        comparison("events",result.events,expectedEvents);
        std::cout << ",\"cpu_and_code_generation_equal\":"
            << (actual.fbrst==recorded.fbrst && actual.vpuStat==recorded.vpuStat && actual.codeGeneration==recorded.codeGeneration?"true":"false")
            << "}\n";
        return matches?0:2;
    }
    catch (const std::exception &e) { std::cerr << "[nexo-vif-replay:error] " << e.what() << '\n'; return 1; }
}
