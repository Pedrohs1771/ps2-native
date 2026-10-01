#include "nexo/iop_capture.h"
#include "iop_compat_test_support.h"
#include "emulator/core/iop_native.h"

#include <cstdlib>
#include <fstream>

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 3) return 2;
    try
    {
        using namespace ps2native::nexo::iop_lab;
        using namespace iop_test;
        const std::filesystem::path root(argv[1]);
        if (std::filesystem::exists(root)) throw std::runtime_error("fixture output already exists");
        std::filesystem::create_directory(root);
        Host host;
        if (argc == 3)
        {
            if (std::string_view(argv[2]) == "collision")
            {
                const auto existing = root / "00000001-module";
                std::filesystem::create_directory(existing);
                std::ofstream(existing / "image.irx") << "preserve";
                setenv("PS2X_IOP_CAPTURE_DIR", root.c_str(), 1);
                const std::array<uint8_t, 1> bytes{0};
                captureModule(host, "collision", bytes, {}, 0);
                require(host.logs.size() == 1, "capture directory collision was not reported");
                require(!std::filesystem::exists(existing / "request.json"), "existing event was completed or reused");
                unsetenv("PS2X_IOP_CAPTURE_DIR");
                return 0;
            }
            if (std::string_view(argv[2]) != "saturate") return 2;
            setenv("PS2X_IOP_CAPTURE_DIR", root.c_str(), 1);
            const RpcRequest empty{};
            for (unsigned index = 0; index < 4099; ++index)
            {
                const auto event = captureRpcRequest(host, empty, index);
                require(event.empty() == (index >= 4096), "RPC capture budget changed");
            }
            require(host.logs.size() == 1, "exhausted RPC budget did not produce exactly one warning");
            const std::array<uint8_t, 1> bytes{0};
            captureModule(host, "after-rpc-limit", bytes, {}, 4100);
            captureFault(host, "after-rpc-limit", bytes, 4101);
            require(host.logs.size() == 1, "RPC traffic exhausted the module/fault budgets");
            require(host.guestReads == 0 && host.guestWrites == 0, "empty capture touched guest RAM");
            unsetenv("PS2X_IOP_CAPTURE_DIR");
            return 0;
        }
        auto rpc = request(0x80000400u, 0xFEu);
        rpc.callToken = 0x100000002ull; rpc.mode = 1;
        rpc.clientAddress = 0x10; rpc.serverAddress = 0x20;
        rpc.serverFunction = 0x30; rpc.serverBuffer = 0x40;
        rpc.send = {0x100, 8}; rpc.endFunction = 0x50; rpc.endParameter = 0x60;
        unsetenv("PS2X_IOP_CAPTURE_DIR");
        require(captureRpcRequest(host, rpc, 11).empty(), "disabled capture produced an event");
        require(host.guestReads == 0 && host.guestWrites == 0, "disabled capture accessed guest memory");
        const auto capture = root / "events";
        setenv("PS2X_IOP_CAPTURE_DIR", capture.c_str(), 1);
        const std::array<uint8_t, 4> bytes{0, 1, 0x80, 0xFF};
        const std::array<uint8_t, 5> args{'a', 0, 'b', 0xFF, 0};
        captureModule(host, "buffer\"\\\n\xFF", bytes, args, 10);
        auto event = captureRpcRequest(host, rpc, 11);
        require(!event.empty(), "request capture failed");
        host.fill(0x800, 16, 0x5A);
        RpcResult result{};
        result.handled = true; result.resultAddress = 0x800;
        result.signalNowaitCompletion = result.signalCompletion = true;
        result.callbackPolicy = CallbackPolicy::Suppress;
        result.serverDispatchPolicy = ServerDispatchPolicy::Suppress;
        result.guestFunction = 0x70; result.guestDefaultResultAddress = 0x80;
        for (unsigned i = 0; i < 4; ++i) result.guestArguments[i] = i + 1;
        captureRpcResult(host, event, rpc, result, 20, 7, 0, false);
        rpc.send = {0xFFFFFFFFu, 8};
        event = captureRpcRequest(host, rpc, 21);
        captureRpcResult(host, event, rpc, {}, 21, 7, 0, false);
        rpc.send = {0, 1024 * 1024 + 1};
        event = captureRpcRequest(host, rpc, 22);
        captureRpcResult(host, event, rpc, {}, 22, 7, 0, false);
        rpc.send = {0xFFFFFFFFu, 0}; rpc.receive = {0xFFFFFFFFu, 0};
        event = captureRpcRequest(host, rpc, 23);
        captureRpcResult(host, event, rpc, result, 23, 7, 0, false);
        captureFault(host, "diagnostic\n\"quoted\"", bytes, 24);
        IopSubsystem subsystem(host);
        require(subsystem.loadModule("rom0:XMCSERV").moduleId > 0, "HLE module setup failed");
        require(subsystem.handleRpc(request(0x80000400u, 0xFEu)).handled, "captured HLE RPC changed its result");
        require(host.word(0x804) == 0x0205 && host.word(0x808) == 0x0206, "captured RPC changed memory");
        require(!subsystem.handleRpc(request(0x12345678u, 1)).handled, "unknown RPC changed its result");
        const ps2x::iop::detail::IopNativeProgram emptyProgram{};
        IopSubsystem strict(host, emptyProgram);
        Irx unseen;
        unseen.words(0, {0x03E00008u, 0u});
        unseen.install(host);
        require(strict.loadModuleBuffer(0x1000).moduleId < 0, "unseen native module was accepted");
        require(strict.debugSnapshot().nativeFaults == 1 &&
                strict.debugSnapshot().interpretedInstructions == 0,
                "native fault capture changed strict dispatch");
        const auto writes = host.guestWrites;
        const auto blocked = root / "not-a-directory";
        std::ofstream(blocked) << "preserve";
        setenv("PS2X_IOP_CAPTURE_DIR", blocked.c_str(), 1);
        captureModule(host, "failure", bytes, args, 25);
        require(captureRpcRequest(host, rpc, 25).empty(), "failed capture returned an event");
        require(host.guestWrites == writes, "recording failure wrote guest memory");
        require(host.logs.size() >= 2, "recording failure was not reported");
        unsetenv("PS2X_IOP_CAPTURE_DIR");
        std::cout << "Capture fixture passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
