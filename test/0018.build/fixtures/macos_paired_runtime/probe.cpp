// Native paired-runtime smoke: static initialization, an OS thread,
// cross-thread exception_ptr, rethrow and actual unwinder landing pads.
// The build driver must select matching generated libc++ headers/archives.
#include <atomic>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
    std::vector<int> initialized_before_main(64u, 7);
    std::atomic<int> unwound_frames{};
    struct cleanup_witness
    {
        ~cleanup_witness() { unwound_frames.fetch_add(1); }
    };
    [[gnu::noinline]] void throw_from_nested_frame()
    {
        cleanup_witness witness;
        std::vector<int> lifetime(32u, 9);
        if(lifetime.front() == 9) { throw std::runtime_error("paired-runtime"); }
    }
}

int main()
{
    if(initialized_before_main.size() != 64u || initialized_before_main.back() != 7) { return 1; }
    bool caught_integer{};
    try { throw 42; }
    catch(int value) { caught_integer = value == 42; }
    if(!caught_integer) { return 6; }
    std::exception_ptr captured;
    std::thread worker([&]
    {
        try { throw_from_nested_frame(); }
        catch(...) { captured = std::current_exception(); }
    });
    worker.join();
    if(!captured || unwound_frames.load() != 1) { return 2; }
    try { std::rethrow_exception(captured); }
    catch(std::runtime_error const& error)
    {
        return std::strcmp(error.what(), "paired-runtime") == 0 ? 0 : 3;
    }
    catch(...) { return 4; }
    return 5;
}
