// Opt-in microbenchmark: compile with -O3 -pthread and the desired -I<tree>/src.
// Compare the same source against old/new runtime headers. Includes CPU time
// only, not process startup, compilation, or a claim about end-to-end VM speed.
#include <uwvm2/runtime/lib/uwvm_runtime_native_stack_guard.h>
#include <cstdio>
#include <ctime>
#include <cstring>

#if (defined(__linux__) || defined(__APPLE__)) && !defined(_WIN32)
using guard = uwvm2::runtime::lib::native_stack::scope;
static long long cpu_ns()
{
    timespec time{};
    if(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) != 0) { return -1; }
    return static_cast<long long>(time.tv_sec) * 1000000000LL + time.tv_nsec;
}
static bool measure(char const* name, unsigned count)
{
    for(unsigned sample{}; sample != 5; ++sample)
    {
        auto const start{cpu_ns()};
        for(unsigned i{}; i != count; ++i)
        {
            guard scope;
            if(!scope.ready()) { return false; }
            asm volatile("" ::: "memory");
        }
        auto const end{cpu_ns()};
        if(start < 0 || end < start) { return false; }
        std::printf("{\"path\":\"%s\",\"sample\":%u,\"entries\":%u,\"cpu_ns_per_entry\":%.2f}\n",
                    name, sample, count, double(end - start) / count);
    }
    return true;
}
static bool run(char const* prefix, unsigned count)
{
    { guard warm; if(!warm.ready()) { return false; } }
    if(!measure(prefix, count)) { return false; }
    guard outer;
    return outer.ready() && measure("nested", count * 100u);
}
static void* worker(void* argument)
{
    auto const count{*static_cast<unsigned*>(argument)};
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(!run("pthread-root", count)));
}
int main(int argc, char** argv)
{
    unsigned count{argc > 1 && std::strcmp(argv[1], "--short") == 0 ? 100u : 10000u};
    if(!run("main-root", count)) { return 1; }
    pthread_t thread{};
    if(pthread_create(&thread, nullptr, worker, &count) != 0) { return 1; }
    void* result{};
    if(pthread_join(thread, &result) != 0 || result != nullptr) { return 1; }
    // An embedding host may already provide an adequate alternate stack.
    auto const bytes{256u * 1024u};
    void* area{mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)};
    if(area == MAP_FAILED) { return 1; }
    stack_t host{}, previous{};
    host.ss_sp = area;
    host.ss_size = bytes;
    if(sigaltstack(&host, &previous) != 0) { munmap(area, bytes); return 1; }
    bool const ok{measure("host-alt-root", count)};
#if defined(__APPLE__)
    if((previous.ss_flags & SS_DISABLE) && previous.ss_size < MINSIGSTKSZ) { previous.ss_size = MINSIGSTKSZ; }
#endif
    if(sigaltstack(&previous, nullptr) != 0) { return 1; }
    return munmap(area, bytes) == 0 && ok ? 0 : 1;
}
#else
int main() { std::puts("native stack entry benchmark: unsupported OS"); }
#endif
