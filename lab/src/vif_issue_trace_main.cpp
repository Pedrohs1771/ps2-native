#include "nexo/vif_capture.h"
#include "vif_case_internal.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv)
{
    using namespace ps2native::nexo;
    try
    {
        if (argc!=3) throw std::invalid_argument("usage: nexo_vif_issue_trace <original-vif-case> <new-trace-directory>");
        const std::filesystem::path directory(argv[1]),output(argv[2]);
        const auto expected=detail::readVifFile(directory/"output-state.nexo",24,detail::vifStateBound);
        const auto events=detail::readVifFile(directory/"events.nexo",24,detail::vifEventBound);
        if (!std::filesystem::create_directory(output)) throw std::invalid_argument("model trace directory must be new");
        // This executable owns its single-threaded process and private runtime.
        // The original parser and default runtime callbacks remain in use.
        if (setenv("PS2X_CAPTURE_SCENE",output.c_str(),1) || setenv("PS2X_CAPTURE_VU_ISSUES","1",1) ||
            setenv("PS2X_CAPTURE_VU_CONTINUOUS","1",1) || setenv("PS2X_FUNCTION_TRACE","0",1))
            throw std::runtime_error("cannot enable process-local issue diagnostics");
        std::ofstream request(output/".vu-request",std::ios::binary); request.put(1); request.close();
        if (!request) throw std::runtime_error("cannot request the first model issue capture");
        VifReplayResult result;
        {
            struct Diagnostics
            {
                std::streambuf* previous=std::cout.rdbuf(std::cerr.rdbuf());
                ~Diagnostics() { std::cout.rdbuf(previous); }
            } diagnostics;
            result=replayVifCase(directory);
        }
        size_t traces=0;
        for (const auto& entry:std::filesystem::directory_iterator(output))
            if (entry.is_directory() && std::filesystem::is_regular_file(entry.path()/".issue-complete")) ++traces;
        binary::Reader reader(events,{'N','E','X','O','V','T','R',0},1,detail::vifEventBound);
        uint32_t count=0; reader(count); if (count>262144 || count>reader.remaining()/33)
            throw std::invalid_argument("invalid observed VIF event count");
        size_t callbacks=0;
        for (uint32_t i=0;i<count;++i)
        {
            uint8_t kind; uint64_t cycle; std::array<uint32_t,5> args; std::vector<uint8_t> bytes;
            reader(kind); reader(cycle); reader(args); reader(bytes); if (kind==1) ++callbacks;
        }
        reader.finish();
        const bool equal=result.state==expected && result.events==events && traces==callbacks && callbacks>0;
        const std::string report="{\"schema\":\"nexo.vif.model.issue.trace.v1\",\"assurance\":\"tested_only\","
            "\"backend\":\"current_runtime_original_callbacks\",\"clock_phase\":\"model_before_issue_after_stalls\","
            "\"matches_original_state_and_events\":"+std::string(equal?"true":"false")+
            ",\"expected_callbacks\":"+std::to_string(callbacks)+",\"completed_traces\":"+std::to_string(traces)+"}\n";
        std::ofstream receipt(output/"report.json"); receipt << report; receipt.close();
        if (!receipt) throw std::runtime_error("cannot publish model issue trace receipt");
        if (equal)
        {
            std::ofstream complete(output/".complete",std::ios::binary); complete.put(1); complete.close();
            if (!complete) throw std::runtime_error("cannot publish completed model issue history");
        }
        std::cout << report; return equal?0:2;
    }
    catch (const std::exception& e) { std::cerr << "[nexo-vif-issues:error] " << e.what() << '\n'; return 1; }
}
