#include "ps2_ee_data_family.h"
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{
    namespace fs=std::filesystem;
    void ordinary(const fs::path &path)
    {
        for(auto item=fs::absolute(path);;item=item.parent_path())
        {
            if(fs::is_symlink(fs::symlink_status(item))) throw std::invalid_argument("linked probe path");
            if(item==item.parent_path()) break;
        }
    }
    std::vector<uint8_t> read(const fs::path &path,size_t maximum)
    {
        ordinary(path);
        if(!fs::is_regular_file(path))
            throw std::invalid_argument("missing probe input");
        const auto bytes=fs::file_size(path);
        if(bytes>maximum)
            throw std::invalid_argument("missing or oversized probe input");
        std::vector<uint8_t> result(static_cast<size_t>(bytes));
        std::ifstream stream(path,std::ios::binary);
        stream.read(reinterpret_cast<char *>(result.data()),static_cast<std::streamsize>(result.size()));
        if(static_cast<size_t>(stream.gcount())!=result.size() || stream.peek()!=std::char_traits<char>::eof())
            throw std::invalid_argument("probe input changed during read");
        return result;
    }
    const char *status(ps2native::ee_family::Status value)
    {
        using S=ps2native::ee_family::Status;
        switch(value)
        {
        case S::Ready:return "Ready";case S::MissingEntry:return "MissingEntry";
        case S::Ambiguous:return "Ambiguous";case S::NoRam:return "NoRam";
        case S::MisalignedPc:return "MisalignedPc";case S::OutsideRam:return "OutsideRam";
        case S::UnsupportedEntryContext:return "UnsupportedEntryContext";
        }
        return "Unknown";
    }
}

int main(int argc,char **argv)
{
    try
    {
        if(argc==2 && std::string_view(argv[1])=="--describe")
        {
            const auto dispatcher=ps2native::ee_family::compiledDispatcher();
            std::cout<<"{\"schema_version\":1,\"available\":"<<(dispatcher?"true":"false")
                     <<",\"families\":"<<(dispatcher?dispatcher->families():0u)
                     <<",\"guest_execution\":false,\"strict_approval\":false}\n";
            return 0;
        }
        if(argc!=4) throw std::invalid_argument("usage: nexo_ee_family_probe ram.bin little-endian-pcs.bin fresh-report.json");
        const auto dispatcher=ps2native::ee_family::compiledDispatcher();
        if(!dispatcher) throw std::invalid_argument("a compiled laboratory family profile is required");
        const auto ram=read(argv[1],PS2_RAM_SIZE),entries=read(argv[2],32768u*4u);
        if(ram.size()!=PS2_RAM_SIZE || entries.empty() || entries.size()%4)
            throw std::invalid_argument("invalid probe RAM or entry dimensions");
        ordinary(argv[3]);
        // C11 exclusive create; failed/repeated publication never truncates evidence.
        std::FILE *output=std::fopen(argv[3],"wx");
        if(!output) throw std::invalid_argument("probe output must be a fresh ordinary file");
        std::fprintf(output,"{\"schema_version\":1,\"strict_approval\":false,\"closure_proved\":false,"
            "\"guest_execution\":false,\"scope\":\"immutable captured RAM lookup with constructed normal context;"
            " fetch, producer, whole machine, fidelity and gameplay unqualified\",\"rows\":[");
        uint64_t ready=0,ambiguous=0,missing=0,other=0,checked=0;
        for(size_t offset=0;offset<entries.size();offset+=4)
        {
            const uint32_t pc=uint32_t(entries[offset])|(uint32_t(entries[offset+1])<<8)|
                (uint32_t(entries[offset+2])<<16)|(uint32_t(entries[offset+3])<<24);
            R5900Context context{};context.pc=pc;
            const auto admission=dispatcher->admitNormalEntry(ram.data(),&context);
            checked+=admission.candidatesChecked;
            using S=ps2native::ee_family::Status;
            if(admission.status==S::Ready) ++ready;
            else if(admission.status==S::Ambiguous) ++ambiguous;
            else if(admission.status==S::MissingEntry) ++missing;
            else ++other;
            std::fprintf(output,"%s{\"pc\":%u,\"base\":%u,\"status\":\"%s\",\"candidates_checked\":%u}",
                offset?",":"",pc,admission.base,status(admission.status),admission.candidatesChecked);
        }
        std::fprintf(output,"],\"families\":%zu,\"ready\":%llu,\"ambiguous\":%llu,\"missing\":%llu,"
            "\"other\":%llu,\"candidate_checks\":%llu}\n",dispatcher->families(),
            static_cast<unsigned long long>(ready),static_cast<unsigned long long>(ambiguous),
            static_cast<unsigned long long>(missing),static_cast<unsigned long long>(other),
            static_cast<unsigned long long>(checked));
        const bool failed=std::ferror(output)!=0;
        const int closed=std::fclose(output);
        if(failed || closed) throw std::runtime_error("failed to publish complete probe evidence");
        std::cout<<"queried="<<entries.size()/4<<" ready="<<ready<<" ambiguous="<<ambiguous<<" missing="<<missing<<"\n";
        return 0;
    }
    catch(const std::exception &error)
    {
        std::cerr<<error.what()<<'\n';return 2;
    }
}
