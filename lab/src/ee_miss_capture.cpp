#include "nexo/ee_miss_capture.h"
#include "nexo/ee_snapshot.h"
#include "nexo/iop_lab_io.h"
#include "ps2_ee_aot.h"

#include <atomic>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ps2native::nexo
{
    namespace
    {
        std::atomic<uint32_t> events{0};
        const char *statusName(ee_aot::Status status)
        {
            switch(status)
            {
            case ee_aot::Status::Ready:return "Ready";
            case ee_aot::Status::MissingEntry:return "MissingEntry";
            case ee_aot::Status::CodeChanged:return "CodeChanged";
            case ee_aot::Status::MisalignedPc:return "MisalignedPc";
            case ee_aot::Status::OutsideRam:return "OutsideRam";
            case ee_aot::Status::NoRam:return "NoRam";
            case ee_aot::Status::UnsupportedEntryContext:return "UnsupportedEntryContext";
            }
            return "InvalidStatus";
        }
        void writeBlob(const std::filesystem::path &path,std::span<const uint8_t> bytes)
        {
            std::ofstream stream(path,std::ios::binary);
            if(!stream || !stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size()))
                throw std::runtime_error("cannot write EE miss artifact");
            stream.close();
            if(!stream) throw std::runtime_error("cannot complete EE miss artifact");
        }
    }

    void captureEeMiss(const uint8_t *ram,const R5900Context *context,const ee_aot::Dispatcher *dispatcher,
                       uint32_t targetPc,uint32_t sourcePc,PS2Runtime::GuestBranchKind kind,
                       std::string_view operation,bool moduleOwnsAddress,std::string_view moduleKey) noexcept
    {
        const char *root=std::getenv("PS2X_EE_MISS_CAPTURE_DIR");
        if(!root || !*root) return;
        try
        {
            const uint32_t event=events.fetch_add(1);
            if(event>=16)
            {
                if(event==16) throw std::runtime_error("EE miss capture event limit exceeded; capture is incomplete");
                return;
            }
            std::ostringstream name;name<<"ee-miss-"<<std::setw(6)<<std::setfill('0')<<event+1;
            std::filesystem::create_directories(root);
            const auto directory=std::filesystem::path(root)/name.str();
            if(!std::filesystem::create_directory(directory))
                throw std::runtime_error("EE miss capture event already exists");
            std::filesystem::permissions(directory,std::filesystem::perms::owner_all);
            std::vector<uint8_t> frozenRam;
            if(ram)
            {
                frozenRam.assign(ram,ram+PS2_RAM_SIZE);
                writeBlob(directory/"ee-ram.bin",frozenRam);
            }
            const uint8_t *observedRam=ram ? frozenRam.data() : nullptr;
            const auto diagnosis=dispatcher ? dispatcher->diagnose(observedRam,targetPc) : ee_aot::Diagnosis{};
            uint32_t base=0,bytes=0;
            if(ram && !(targetPc&3u) && targetPc<PS2_RAM_SIZE)
            {
                base=targetPc&~0xFFFFu;
                if(targetPc-base>=65536u-512u) base+=32768u;
                bytes=std::min(65536u,PS2_RAM_SIZE-base);
                writeBlob(directory/"snapshot.bin",std::span(observedRam+base,bytes));
            }
            if(context) writeBlob(directory/"ee-context.bin",EeSnapshotCodec::encode(*context));
            std::ostringstream json;
            json<<"{\"schema_version\":1,\"processor\":\"EE\",\"runtime_admission\":\"missing\",";
            json<<"\"target_pc\":"<<targetPc<<",\"source_pc\":"<<sourcePc
                <<",\"branch_kind\":"<<static_cast<uint32_t>(kind)
                <<",\"operation\":"<<iop_lab::jsonString(operation.substr(0,4096))
                <<",\"operation_truncated\":"<<(operation.size()>4096 ? "true":"false")
                <<",\"module_owns_address\":"<<(moduleOwnsAddress ? "true":"false")
                <<",\"module_key\":"<<iop_lab::jsonString(moduleKey.substr(0,4096))
                <<",\"module_key_truncated\":"<<(moduleKey.size()>4096 ? "true":"false")
                <<",\"aot_directory_available\":"<<(dispatcher ? "true":"false")
                <<",\"overlay_lookup_status\":"<<iop_lab::jsonString(dispatcher ? statusName(diagnosis.lookup.status):"AotDisabled")
                <<",\"versions_checked\":"<<diagnosis.lookup.versionsChecked
                <<",\"ram_captured\":"<<(ram ? "true":"false")
                <<",\"context_captured\":"<<(context ? "true":"false")
                <<",\"ee_model_profile\":1,\"window_base\":"<<base<<",\"window_bytes\":"<<bytes
                <<",\"quiescence_qualified\":false,\"complete_machine_checkpoint\":false,\"candidates\":[";
            for(size_t index=0;index<diagnosis.candidates.size();++index)
            {
                const auto &candidate=diagnosis.candidates[index];
                const auto file="expected-"+std::to_string(index)+".bin";
                writeBlob(directory/file,candidate.expected);
                if(index) json<<',';
                json<<"{\"bank_index\":"<<candidate.bank<<",\"source_begin\":"<<candidate.sourceBegin
                    <<",\"source_bytes\":"<<candidate.expected.size()<<",\"expected_file\":"<<iop_lab::jsonString(file)
                    <<",\"mismatch_bytes\":"<<candidate.mismatchBytes<<",\"first_mismatch_offset\":";
                if(candidate.mismatchBytes) json<<candidate.firstMismatchOffset;else json<<"null";
                json<<'}';
            }
            json<<"],\"complete\":true,\"scope\":\"guard diagnosis from captured RAM and optional EE model context; fetch, writer quiescence, kernel/device state, closure and fidelity unqualified\"}\n";
            const auto text=json.str();
            writeBlob(directory/"request.json",std::span(reinterpret_cast<const uint8_t*>(text.data()),text.size()));
            std::cerr<<"[EE:MISS_CAPTURE] event="<<event+1<<" status="
                     <<(dispatcher ? statusName(diagnosis.lookup.status):"AotDisabled")
                     <<" candidates="<<diagnosis.candidates.size()<<'\n';
        }
        catch(const std::exception &error) { std::cerr<<"[EE:MISS_CAPTURE_FAILED] "<<error.what()<<'\n'; }
        catch(...) { std::cerr<<"[EE:MISS_CAPTURE_FAILED] unknown capture failure\n"; }
    }
}
