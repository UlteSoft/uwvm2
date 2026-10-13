// Bounded diagnostic WASIp1 ABI observations. No pause, native PC, guest
// memory borrow, callback, OS handle or execution/restore authority is carried.
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <mutex>
# include <fast_io.h>
# include <fast_io_dsal/array.h>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/vector.h>
# include "wasip1_state.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::wasip1_calls
{
    struct record
    {
        ::std::uint64_t sequence{}, call{};
        ::fast_io::array<char, 64u> name{};
        ::fast_io::array<::std::uint64_t, 16u> arguments{};
        ::fast_io::array<unsigned char, 16u> widths{};
        ::std::size_t name_size{}, count{};
        bool returned{}, has_errno{};
        ::std::uint64_t result{}, participant{}, module{}, session{}, environment{}, created_descriptor{};
        bool context_available{}, descriptor_created{};
    };
    struct view
    {
        bool enabled{}, gap{};
        ::std::uint64_t overwritten{}, oldest{}, newest{}, next{}, remaining{};
        ::fast_io::array<char, 64u> filter{}; ::std::size_t filter_size{};
        ::fast_io::vector<record> records{};
    };
    struct token { ::std::uint64_t call{}, episode{}; record entered{}; };
    inline ::std::atomic<bool> enabled{};
    inline ::std::mutex mutex{};
    inline ::fast_io::array<record, 512u> records{};
    inline ::fast_io::array<char, 64u> filter{};
    inline ::std::size_t filter_size{}, size{}, begin{};
    inline ::std::uint64_t sequence{}, overwritten{}, episode{1u};
    struct thread_labels { ::std::uint64_t participant{}, module{}, session{}; };
    inline thread_local thread_labels current_labels{};
    inline thread_local record* active_record{};
    inline ::std::atomic<::std::uint64_t> session_id{1u};
    inline bool session_started{};
    inline ::std::atomic<::std::uint64_t> environment_sequence{1u};
    [[nodiscard]] inline ::std::uint64_t new_environment() noexcept
    {
        auto value{environment_sequence.load(::std::memory_order_relaxed)};
        while(value != UINT64_MAX)
        { if(environment_sequence.compare_exchange_weak(value,value+1u,::std::memory_order_relaxed)) { return value; } }
        return 0u;
    }
    // Only the genuine observer on the execution thread supplies these labels.
    // They are diagnostics, never a stopped-state or host-access capability.
    inline void observe_thread(::std::uint64_t participant,::std::uint64_t module) noexcept
    { current_labels={participant,module,session_id.load(::std::memory_order_acquire)}; }
    inline void begin_debug_session() noexcept
    {
        ::std::lock_guard lock{mutex}; size=begin=0u;overwritten=0u;
        if(episode==UINT64_MAX) { enabled.store(false,::std::memory_order_release);return; }
        ++episode;session_id.store(episode,::std::memory_order_release);
        if(!session_started) { session_started=true;enabled.store(true,::std::memory_order_release); }
    }
    struct call_scope
    {
        record* previous{};
        explicit call_scope(token& call) noexcept : previous{active_record} { active_record=call.call ? ::std::addressof(call.entered) : nullptr; }
        ~call_scope() { active_record=previous; }
        call_scope(call_scope const&)=delete;
    };
    inline void created_descriptor(::std::uint64_t fd) noexcept
    { if(active_record!=nullptr) { active_record->created_descriptor=fd;active_record->descriptor_created=true; } }
    [[nodiscard]] inline bool uses_descriptor(record const& row,::std::uint64_t fd) noexcept
    {
        auto name{::fast_io::string_view{row.name.data(),row.name_size}};
        if(name.ends_with("_wasm64")) { name=name.subview(0u,name.size()-7u); }
        auto arg{[&](::std::size_t i) { return i<row.count && row.arguments[i]==fd; }};
        if(row.descriptor_created && row.created_descriptor==fd) { return true; }
        if(name=="path_symlink") { return arg(2u); }
        if(name=="path_link") { return arg(0u) || arg(4u); }
        if(name=="path_rename") { return arg(0u) || arg(3u); }
        if(name=="fd_renumber") { return arg(0u) || arg(1u); }
        return (name.starts_with("fd_") || name.starts_with("path_") || name.starts_with("sock_")) && arg(0u);
    }
    [[nodiscard]] inline constexpr bool is_trace(wasip1_state::action action) noexcept
    {
        using enum wasip1_state::action;
        return action == trace_enable || action == trace_disable || action == trace_clear || action == trace_read;
    }
    [[nodiscard]] inline constexpr ::fast_io::string_view errno_name(::std::uint64_t value) noexcept
    {
        constexpr ::fast_io::array<::fast_io::string_view, 77u> names{"esuccess", "e2big", "eacces", "eaddrinuse", "eaddrnotavail", "eafnosupport", "eagain", "ealready", "ebadf", "ebadmsg", "ebusy", "ecanceled", "echild", "econnaborted", "econnrefused", "econnreset", "edeadlk", "edestaddrreq", "edom", "edquot", "eexist", "efault", "efbig", "ehostunreach", "eidrm", "eilseq", "einprogress", "eintr", "einval", "eio", "eisconn", "eisdir", "eloop", "emfile", "emlink", "emsgsize", "emultihop", "enametoolong", "enetdown", "enetreset", "enetunreach", "enfile", "enobufs", "enodev", "enoent", "enoexec", "enolck", "enolink", "enomem", "enomsg", "enoprotoopt", "enospc", "enosys", "enotconn", "enotdir", "enotempty", "enotrecoverable", "enotsock", "enotsup", "enotty", "enxio", "eoverflow", "eownerdead", "eperm", "epipe", "eproto", "eprotonosupport", "eprototype", "erange", "erofs", "espipe", "esrch", "estale", "etimedout", "etxtbsy", "exdev", "enotcapable"};
        return value < names.size() ? names[static_cast<::std::size_t>(value)] : ::fast_io::string_view{"unknown"};
    }
    inline void append_locked(record row) noexcept
    {
        if(sequence == UINT64_MAX) { enabled.store(false, ::std::memory_order_release); return; }
        row.sequence = ++sequence;
        if(size == records.size()) { records[begin] = row; begin = (begin + 1u) % records.size(); ++overwritten; }
        else { records[(begin + size) % records.size()] = row; ++size; }
    }
    [[nodiscard]] inline token enter(::fast_io::u8string_view name, record row) noexcept
    {
        if(!enabled.load(::std::memory_order_relaxed)) { return {}; }
        if(name.empty() || name.size() >= row.name.size() || row.count > row.arguments.size()) { return {}; }
        ::std::lock_guard lock{mutex};
        if(!enabled.load(::std::memory_order_relaxed) || sequence == UINT64_MAX) { return {}; }
        if(filter_size != 0u)
        {
            if(name.size() != filter_size) { return {}; }
            for(::std::size_t i{}; i != filter_size; ++i) { if(name[i] != static_cast<char8_t>(filter[i])) { return {}; } }
        }
        for(char8_t c : name)
        { if(!((c >= u8'a' && c <= u8'z') || (c >= u8'0' && c <= u8'9') || c == u8'_')) { return {}; } }
        row.name_size = name.size();
        for(::std::size_t i{}; i != name.size(); ++i) { row.name[i] = static_cast<char>(name[i]); }
        row.call = sequence + 1u; row.returned = row.has_errno = false;
        row.session=episode;row.context_available=current_labels.session==episode && current_labels.participant!=0u;
        if(row.context_available) { row.participant=current_labels.participant;row.module=current_labels.module; }
        append_locked(row); return {row.call, episode, row};
    }
    inline void leave(token const& call, bool has_errno, ::std::uint64_t result) noexcept
    {
        if(call.call == 0u) { return; }
        ::std::lock_guard lock{mutex};
        if(call.episode != episode) { return; } // clear retires in-flight diagnostic pairs
        auto row{call.entered}; row.returned = true; row.has_errno = has_errno; row.result = result; append_locked(row);
    }
    [[nodiscard]] inline view apply(wasip1_state::request const& request, ::std::uint64_t environment_id = 0u)
    {
        if(!wasip1_state::valid(request) || (!is_trace(request.operation) && request.operation!=wasip1_state::action::descriptor_detail)) { return {}; }
        ::std::lock_guard lock{mutex};
        using enum wasip1_state::action;
        if(request.operation == trace_enable)
        {
            filter_size = request.name.size();
            for(::std::size_t i{}; i != filter_size; ++i) { filter[i] = static_cast<char>(request.name[i]); }
            enabled.store(true, ::std::memory_order_release);
        }
        else if(request.operation == trace_disable) { enabled.store(false, ::std::memory_order_release); }
        else if(request.operation == trace_clear)
        {
            size = begin = 0u; overwritten = 0u;
            if(episode != UINT64_MAX) { ++episode;session_id.store(episode,::std::memory_order_release); }
            else { enabled.store(false, ::std::memory_order_release); }
        }
        view out{}; out.enabled = enabled.load(::std::memory_order_relaxed);
        out.filter = filter; out.filter_size = filter_size; out.overwritten = overwritten;
        out.oldest = size == 0u ? 0u : records[begin].sequence; out.newest = sequence; out.next = request.first;
        out.gap = size != 0u && request.first < out.oldest - 1u;
        if(request.operation == trace_read || request.operation==descriptor_detail)
        {
            out.records.reserve(static_cast<::std::size_t>(request.count));
            for(::std::size_t i{}; i != size; ++i)
            {
                auto const& row{records[(begin + i) % records.size()]};
                if(row.sequence <= request.first) { continue; }
                if(request.operation==descriptor_detail && ((environment_id!=0u ? row.environment!=environment_id : !row.context_available || row.module!=request.module) || !uses_descriptor(row,request.descriptor))) { continue; }
                if(out.records.size() == request.count) { ++out.remaining; continue; }
                out.records.push_back(row); out.next = row.sequence;
            }
        }
        return out;
    }
    template<typename Output> inline void print(Output output, view const& copied)
    {
        if(copied.filter_size > 63u || copied.records.size() > 64u)
        { ::fast_io::io::print(output, "error: WASIp1 trace resource limit\n"); return; }
        for(auto const& row : copied.records)
        {
            if(row.name_size == 0u || row.name_size > 63u || row.count > 16u)
            { ::fast_io::io::print(output, "error: WASIp1 trace resource limit\n"); return; }
        }
        ::fast_io::io::print(output, "wasip1-trace enabled=", copied.enabled, " filter=",
            copied.filter_size == 0u ? ::fast_io::string_view{"all"} : ::fast_io::string_view{copied.filter.data(), copied.filter_size},
            " overwritten=", ::fast_io::mnp::dec(copied.overwritten), " oldest=", ::fast_io::mnp::dec(copied.oldest),
            " newest=", ::fast_io::mnp::dec(copied.newest), " next-after=", ::fast_io::mnp::dec(copied.next),
            " remaining=", ::fast_io::mnp::dec(copied.remaining), " cursor-gap=", copied.gap, "\n");
        for(auto const& row : copied.records)
        {
            ::fast_io::io::print(output, "  seq=", ::fast_io::mnp::dec(row.sequence), " call=", ::fast_io::mnp::dec(row.call),
                " phase=", row.returned ? ::fast_io::string_view{"return"} : ::fast_io::string_view{"entry"}, " name=", ::fast_io::string_view{row.name.data(), row.name_size});
            if(row.context_available) { ::fast_io::io::print(output," thread=",row.participant," module=",row.module," session=",row.session); }
            else { ::fast_io::io::print(output," thread=unavailable module=unavailable"); }
            if(row.descriptor_created) { ::fast_io::io::print(output," created-fd=",row.created_descriptor); }
            if(row.returned)
            {
                if(row.has_errno) { ::fast_io::io::print(output, " errno=", ::fast_io::mnp::dec(row.result), " errno-name=", errno_name(row.result)); }
                else { ::fast_io::io::print(output, " result=void"); }
            }
            else
            {
                ::fast_io::io::print(output, " argc=", ::fast_io::mnp::dec(row.count));
                for(::std::size_t i{}; i != row.count; ++i)
                { ::fast_io::io::print(output, " arg", ::fast_io::mnp::dec(i), "=i", ::fast_io::mnp::dec(row.widths[i]), ":", ::fast_io::mnp::hex0x(row.arguments[i])); }
            }
            ::fast_io::io::print(output, "\n");
        }
        ::fast_io::io::print(output, "wasip1-trace end\n");
    }
}
