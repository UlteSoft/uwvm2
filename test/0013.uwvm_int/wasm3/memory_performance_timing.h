#pragma once
// Linux sandbox timing diagnostics. The guest-only wall interval remains the
// primary measurement. Thread CPU time and per-thread scheduling/fault deltas
// distinguish execution work from descheduling without discarding any sample.
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sys/resource.h>
#include <time.h>

struct memory_performance_timer
{
    struct sample
    {
        long long wall_ns{}, cpu_ns{};
        long voluntary{}, involuntary{}, minor_faults{}, major_faults{};
    };
    std::array<sample, 100> measurements{};
    rusage usage_begin{};
    timespec cpu_begin{};
    std::chrono::steady_clock::time_point wall_begin{};

    void begin()
    {
        if(::getrusage(RUSAGE_THREAD, &usage_begin) != 0 ||
           ::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_begin) != 0) { std::abort(); }
        wall_begin = std::chrono::steady_clock::now();
    }

    void end(unsigned index, bool record)
    {
        auto const wall_end{std::chrono::steady_clock::now()};
        timespec cpu_end{};
        rusage usage_end{};
        if(::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_end) != 0 ||
           ::getrusage(RUSAGE_THREAD, &usage_end) != 0) { std::abort(); }
        if(!record) { return; }
        if(index >= measurements.size()) { std::abort(); }
        measurements[index] = {
            static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(wall_end - wall_begin).count()),
            (static_cast<long long>(cpu_end.tv_sec) - static_cast<long long>(cpu_begin.tv_sec)) * 1000000000ll +
                static_cast<long long>(cpu_end.tv_nsec) - static_cast<long long>(cpu_begin.tv_nsec),
            usage_end.ru_nvcsw - usage_begin.ru_nvcsw, usage_end.ru_nivcsw - usage_begin.ru_nivcsw,
            usage_end.ru_minflt - usage_begin.ru_minflt, usage_end.ru_majflt - usage_begin.ru_majflt};
    }

    void print(unsigned count) const
    {
        if(count > measurements.size()) { std::abort(); }
        std::printf(",\"nanoseconds\":[");
        for(unsigned i{}; i != count; ++i) { std::printf("%s%lld", i ? "," : "", measurements[i].wall_ns); }
        std::printf("],\"thread_cpu_nanoseconds\":[");
        for(unsigned i{}; i != count; ++i) { std::printf("%s%lld", i ? "," : "", measurements[i].cpu_ns); }
        std::printf("],\"guest_diagnostics\":[");
        for(unsigned i{}; i != count; ++i)
        {
            auto const& value{measurements[i]};
            std::printf("%s{\"voluntary\":%ld,\"involuntary\":%ld,\"minor_faults\":%ld,\"major_faults\":%ld}",
                        i ? "," : "", value.voluntary, value.involuntary, value.minor_faults, value.major_faults);
        }
        std::puts("]}");
    }
};
