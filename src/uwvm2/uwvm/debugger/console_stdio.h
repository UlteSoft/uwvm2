// Host console output sink. The WASI environment retains only this callback;
// guest threads never touch the terminal renderer or controller.
#pragma once
#include <fast_io.h>
#include <fast_io_dsal/string_view.h>
#include <atomic>
#include <mutex>
#include <string>
#include <utility>
#include <cstring>
namespace uwvm2::uwvm::debugger::console_stdio
{
    inline ::std::atomic_bool enabled{};
    inline ::std::mutex mutex{};
    inline ::std::string pending{}, input{};
    inline constexpr ::std::size_t capacity{32768u};
    [[nodiscard]] inline ::std::string timestamp() noexcept
    {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try { return ::fast_io::concat_std("[",::fast_io::local(::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::realtime)),"] "); }
        catch(...) { return "[clock unavailable] "; }
#else
        return ::fast_io::concat_std("[",::fast_io::local(::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::realtime)),"] ");
#endif
    }
    inline void clear() { ::std::lock_guard lock{mutex};pending.clear(); }
    inline bool write(int stream, ::fast_io::io_scatter_t const* vectors, ::std::size_t count) noexcept
    {
        if(!enabled.load(::std::memory_order_acquire) || (stream!=1 && stream!=2)) { return false; }
        ::std::lock_guard lock{mutex};
        if(pending.size()>capacity-128u) { pending="[earlier guest output omitted]\n"; }
        if(!pending.empty() && pending.back()!='\n') { pending.push_back('\n'); }
        pending.append(timestamp());pending.append(stream==1 ? "[stdout] " : "[stderr] ");
        for(::std::size_t i{}; i<count; ++i)
        {
            auto const available{capacity-pending.size()};
            auto const take{::std::min(vectors[i].len,available)};
            pending.append(static_cast<char const*>(vectors[i].base),take);
            if(take!=vectors[i].len)
            { pending.resize(capacity-28u); pending.append("\n[guest output truncated]\n"); break; }
        }
        return true;
    }
    inline bool queue_input(::fast_io::string_view text)
    {
        ::std::lock_guard lock{mutex};
        if(text.size()+1u>capacity-input.size()) { return false; }
        input.append(text.data(),text.size()); input.push_back('\n'); return true;
    }
    inline bool read(int stream, ::fast_io::io_scatter_t const* vectors, ::std::size_t count, ::std::size_t& consumed) noexcept
    {
        if(stream!=0) { return false; }
        ::std::lock_guard lock{mutex}; consumed=0u;
        // Debug stdin is a bounded input document, supplied with `input TEXT`.
        // Empty input means EOF; it can never consume debugger command bytes.
        for(::std::size_t i{}; i<count && consumed<input.size(); ++i)
        {
            auto const n{::std::min(vectors[i].len,input.size()-consumed)};
            ::std::memcpy(const_cast<void*>(vectors[i].base),input.data()+consumed,n); consumed+=n;
        }
        if(consumed!=0u && enabled.load(::std::memory_order_acquire))
        {
            if(pending.size()+consumed+128u>capacity) { pending.clear(); }
            pending.append(timestamp());pending.append("[stdin] "); pending.append(input.data(),::std::min(consumed,capacity-pending.size()));
        }
        input.erase(0u,consumed); return true;
    }
    [[nodiscard]] inline ::std::string take()
    { ::std::lock_guard lock{mutex}; ::std::string text{}; text.swap(pending); return text; }
}
