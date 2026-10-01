#include "nexo/iop_capture.h"
#include "nexo/iop_lab_io.h"

#include <atomic>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace ps2native::nexo::iop_lab
{
    namespace
    {
        constexpr uint32_t maxPayloadBytes = 1024u * 1024u;
        std::atomic<uint64_t> nextEvent{1u};
        std::atomic<uint64_t> rpcEvents{0u};
        std::atomic<uint64_t> moduleEvents{0u};
        std::atomic<uint64_t> faultEvents{0u};

        std::filesystem::path createEvent(std::string_view kind)
        {
            const char *root = std::getenv("PS2X_IOP_CAPTURE_DIR");
            if (!root || !*root) return {};
            auto *counter = kind == "rpc" ? &rpcEvents : kind == "module" ? &moduleEvents : &faultEvents;
            const uint64_t limit = kind == "rpc" ? 4096u : kind == "module" ? 256u : 16u;
            const uint64_t event = counter->fetch_add(1u);
            if (event >= limit)
            {
                if (event == limit)
                    throw std::runtime_error(std::string("IOP capture ") + std::string(kind) +
                        " event limit exceeded; capture is incomplete");
                return {}; // One diagnostic per exhausted kind; preserve other event budgets.
            }
            const uint64_t sequence = nextEvent.fetch_add(1u);
            std::ostringstream name;
            name << std::setfill('0') << std::setw(8) << sequence << '-' << kind;
            std::filesystem::create_directories(root);
            const auto directory = std::filesystem::path(root) / name.str();
            if (!std::filesystem::create_directory(directory))
                throw std::runtime_error("IOP capture refuses an existing event directory");
            return directory;
        }

        void writeJson(const std::filesystem::path &path, const std::string &contents)
        {
            std::ofstream output(path);
            if (!output || !output.write(contents.data(), static_cast<std::streamsize>(contents.size())))
                throw std::runtime_error("cannot write IOP capture metadata");
            output.close();
            if (!output) throw std::runtime_error("cannot complete IOP capture metadata");
        }

        void reportFailure(ps2x::iop::IopHost &host, std::string_view error) noexcept
        {
            try
            {
                host.log(ps2x::iop::LogLevel::Warning,
                         std::string("[IOP:capture-incomplete] ") + std::string(error));
            }
            catch (...) {} // A broken diagnostic sink must not alter guest execution.
        }

        void writeBlob(const std::filesystem::path &path, std::span<const uint8_t> bytes)
        {
            std::ofstream output(path, std::ios::binary);
            if (!output || (bytes.size() && !output.write(
                    reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()))))
                throw std::runtime_error("cannot write IOP capture bytes");
            output.close();
            if (!output) throw std::runtime_error("cannot complete IOP capture bytes");
        }

        bool captureBuffer(ps2x::iop::IopHost &host, ps2x::iop::GuestBuffer buffer,
                           const std::filesystem::path &path)
        {
            if (buffer.size > maxPayloadBytes) return false;
            std::vector<uint8_t> bytes(buffer.size);
            if (buffer.size && !host.readGuest(buffer.address, bytes.data(), bytes.size())) return false;
            writeBlob(path, bytes);
            return true;
        }
    }

    void captureModule(ps2x::iop::IopHost &host, std::string_view path,
                       std::span<const uint8_t> image, std::span<const uint8_t> arguments,
                       uint64_t cycles)
    {
        try
        {
            const auto directory = createEvent("module");
            if (directory.empty()) return;
            if (image.empty() || image.size() > 64u * 1024u * 1024u || arguments.size() > 64u * 1024u)
                throw std::runtime_error("IOP module capture input outside bounds");
            writeBlob(directory / "image.irx", image);
            writeBlob(directory / "arguments.bin", arguments);
            std::ostringstream json;
            json << "{\"schema_version\":1,\"kind\":\"module\",\"path\":" << jsonString(path)
                 << ",\"iop_cycles_before\":" << cycles << ",\"image_bytes\":" << image.size()
                 << ",\"argument_bytes\":" << arguments.size()
                 << ",\"scope\":\"observed loader input only; no closure or fidelity qualification\"}\n";
            writeJson(directory / "request.json", json.str());
        }
        catch (const std::exception &error) { reportFailure(host, error.what()); }
        catch (...) { reportFailure(host, "unknown module capture failure"); }
    }

    std::filesystem::path captureRpcRequest(ps2x::iop::IopHost &host,
                                           const ps2x::iop::RpcRequest &request, uint64_t cycles)
    {
        try
        {
            const auto directory = createEvent("rpc");
            if (directory.empty()) return {};
            const bool captured = captureBuffer(host, request.send, directory / "send.bin");
            std::ostringstream json;
            json << "{\"schema_version\":1,\"kind\":\"rpc\",\"iop_cycles_before\":" << cycles
                 << ",\"call_token\":" << request.callToken << ",\"sid\":" << request.sid
                 << ",\"function\":" << request.function << ",\"mode\":" << request.mode
                 << ",\"client_address\":" << request.clientAddress
                 << ",\"server_address\":" << request.serverAddress
                 << ",\"server_function\":" << request.serverFunction
                 << ",\"server_buffer\":" << request.serverBuffer
                 << ",\"send_address\":" << request.send.address << ",\"send_bytes\":" << request.send.size
                 << ",\"receive_address\":" << request.receive.address << ",\"receive_bytes\":" << request.receive.size
                 << ",\"end_function\":" << request.endFunction << ",\"end_parameter\":" << request.endParameter
                 << ",\"send_captured\":" << (captured ? "true" : "false")
                 << ",\"scope\":\"observed RPC only; kernel state, EE execution and service fidelity unqualified\"}\n";
            writeJson(directory / "request.json", json.str());
            return directory;
        }
        catch (const std::exception &error) { reportFailure(host, error.what()); return {}; }
        catch (...) { reportFailure(host, "unknown RPC request capture failure"); return {}; }
    }

    void captureRpcResult(ps2x::iop::IopHost &host, const std::filesystem::path &directory,
                          const ps2x::iop::RpcRequest &request, const ps2x::iop::RpcResult &result,
                          uint64_t cycles, uint64_t nativeInstructions, uint64_t interpretedInstructions,
                          bool nativeFault)
    {
        if (directory.empty()) return;
        try
        {
            const bool captured = result.handled && captureBuffer(host, request.receive, directory / "receive.bin");
            std::ostringstream json;
            json << "{\"schema_version\":1,\"kind\":\"rpc-result\",\"iop_cycles_after\":" << cycles
                 << ",\"handled\":" << (result.handled ? "true" : "false")
                 << ",\"result_address\":" << result.resultAddress
                 << ",\"signal_nowait_completion\":" << (result.signalNowaitCompletion ? "true" : "false")
                 << ",\"signal_completion\":" << (result.signalCompletion ? "true" : "false")
                 << ",\"callback_policy\":" << static_cast<uint32_t>(result.callbackPolicy)
                 << ",\"server_dispatch_policy\":" << static_cast<uint32_t>(result.serverDispatchPolicy)
                 << ",\"guest_function\":" << result.guestFunction
                 << ",\"guest_default_result_address\":" << result.guestDefaultResultAddress
                 << ",\"guest_arguments\":[" << result.guestArguments[0] << ',' << result.guestArguments[1]
                 << ',' << result.guestArguments[2] << ',' << result.guestArguments[3] << ']'
                 << ",\"native_instructions\":" << nativeInstructions
                 << ",\"interpreted_instructions\":" << interpretedInstructions
                 << ",\"native_fault\":" << (nativeFault ? "true" : "false")
                 << ",\"receive_captured\":" << (captured ? "true" : "false") << "}\n";
            writeJson(directory / "result.json", json.str());
        }
        catch (const std::exception &error) { reportFailure(host, error.what()); }
        catch (...) { reportFailure(host, "unknown RPC result capture failure"); }
    }

    void captureFault(ps2x::iop::IopHost &host, std::string_view diagnostic,
                      std::span<const uint8_t> ram, uint64_t cycles)
    {
        try
        {
            const auto directory = createEvent("fault");
            if (directory.empty()) return;
            writeBlob(directory / "iop-ram.bin", ram);
            std::ostringstream json;
            json << "{\"schema_version\":1,\"kind\":\"fault\",\"iop_cycles\":" << cycles
                 << ",\"diagnostic\":" << jsonString(diagnostic)
                 << ",\"scope\":\"RAM diagnostic only; not a canonical full state\"}\n";
            writeJson(directory / "request.json", json.str());
        }
        catch (const std::exception &error) { reportFailure(host, error.what()); }
        catch (...) { reportFailure(host, "unknown fault capture failure"); }
    }
}
