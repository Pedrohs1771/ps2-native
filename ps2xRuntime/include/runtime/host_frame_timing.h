#pragma once

#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace ps2x::runtime
{
    class ScopedSigtermStopRequest
    {
    public:
        ScopedSigtermStopRequest() noexcept
        {
            s_requested = 0;
            m_previousHandler = std::signal(SIGTERM, &handleSignal);
            m_installed = m_previousHandler != SIG_ERR;
        }

        ~ScopedSigtermStopRequest()
        {
            if (m_installed)
            {
                std::signal(SIGTERM, m_previousHandler);
            }
        }

        ScopedSigtermStopRequest(const ScopedSigtermStopRequest &) = delete;
        ScopedSigtermStopRequest &operator=(const ScopedSigtermStopRequest &) = delete;

        [[nodiscard]] bool installed() const noexcept
        {
            return m_installed;
        }

        [[nodiscard]] bool requested() const noexcept
        {
            return s_requested != 0;
        }

    private:
        static void handleSignal(int) noexcept
        {
            s_requested = 1;
        }

        inline static volatile std::sig_atomic_t s_requested = 0;
        using SignalHandler = void (*)(int);
        SignalHandler m_previousHandler = SIG_ERR;
        bool m_installed = false;
    };

    class HostFrameTimingRecorder
    {
    public:
        using Clock = std::chrono::steady_clock;

        void record(Clock::time_point started,
                    Clock::time_point completed,
                    uint64_t guestVsyncTick)
        {
            if (completed < started)
            {
                throw std::invalid_argument("host frame completion precedes its start");
            }

            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(completed - started);
            m_samples.push_back({static_cast<uint64_t>(m_samples.size() + 1u),
                                 static_cast<uint64_t>(elapsed.count()),
                                 guestVsyncTick});
        }

        [[nodiscard]] size_t sampleCount() const noexcept
        {
            return m_samples.size();
        }

        void writeCsv(std::ostream &output) const
        {
            output << "frame,elapsed_ns,guest_vsync_tick\n";
            for (const Sample &sample : m_samples)
            {
                output << sample.frame << ',' << sample.elapsedNanoseconds << ','
                       << sample.guestVsyncTick << '\n';
            }
        }

    private:
        struct Sample
        {
            uint64_t frame;
            uint64_t elapsedNanoseconds;
            uint64_t guestVsyncTick;
        };

        std::vector<Sample> m_samples;
    };
}
