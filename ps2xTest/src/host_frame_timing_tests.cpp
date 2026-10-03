#include "MiniTest.h"
#include "runtime/host_frame_timing.h"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

void register_host_frame_timing_tests()
{
    MiniTest::Case("HostFrameTiming", [](TestCase &tc)
    {
        tc.Run("records host intervals and guest VSYNC ticks as CSV", [](TestCase &t)
        {
            using Recorder = ps2x::runtime::HostFrameTimingRecorder;
            const Recorder::Clock::time_point base{};
            Recorder recorder;
            recorder.record(base, base + std::chrono::nanoseconds(16666667), 1u);
            recorder.record(base + std::chrono::nanoseconds(16666667),
                            base + std::chrono::nanoseconds(33333334), 2u);
            recorder.record(base + std::chrono::nanoseconds(33333334),
                            base + std::chrono::nanoseconds(50000000), 2u);

            std::ostringstream csv;
            recorder.writeCsv(csv);
            t.Equals(recorder.sampleCount(), size_t{3}, "records each host presentation interval");
            t.Equals(csv.str(),
                     std::string("frame,elapsed_ns,guest_vsync_tick\n"
                                 "1,16666667,1\n"
                                 "2,16666667,2\n"
                                 "3,16666666,2\n"),
                     "CSV retains frame durations independently of guest VSYNC tick cadence");
        });

        tc.Run("SIGTERM requests graceful stop and leaves frame samples serializable", [](TestCase &t)
        {
            using Recorder = ps2x::runtime::HostFrameTimingRecorder;
            using StopRequest = ps2x::runtime::ScopedSigtermStopRequest;
            const Recorder::Clock::time_point base{};
            Recorder recorder;
            recorder.record(base, base + std::chrono::milliseconds(16), 7u);

            std::ostringstream csv;
            {
                StopRequest stopRequest;
                t.IsTrue(stopRequest.installed(), "installs a scoped SIGTERM handler");
                t.IsFalse(stopRequest.requested(), "starts without a pending termination request");
                t.Equals(std::raise(SIGTERM), 0, "SIGTERM is handled without terminating the test process");
                t.IsTrue(stopRequest.requested(), "signal handler publishes a graceful stop request");
                if (stopRequest.requested())
                {
                    recorder.writeCsv(csv);
                }
            }
            t.Equals(csv.str(),
                     std::string("frame,elapsed_ns,guest_vsync_tick\n1,16000000,7\n"),
                     "frame samples remain available for the runtime shutdown writer");
        });

        tc.Run("rejects a reversed host frame interval", [](TestCase &t)
        {
            using Recorder = ps2x::runtime::HostFrameTimingRecorder;
            const Recorder::Clock::time_point base{};
            Recorder recorder;
            bool rejected = false;
            try
            {
                recorder.record(base + std::chrono::nanoseconds(1), base, 0u);
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            t.IsTrue(rejected, "invalid timing intervals are not recorded as frame data");
            t.Equals(recorder.sampleCount(), size_t{0}, "invalid intervals leave the sample set unchanged");
        });
    });
}
