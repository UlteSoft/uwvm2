/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <cerrno>
# include <cstdint>
# include <cstring>
# include <utility>
# include <fast_io.h>
# if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  include "posix_abi.h"
# endif
# if defined(__APPLE__) && defined(__MACH__)
#  include <libproc.h>
#  include <sys/proc_info.h>
#  include <sys/ttycom.h>
# endif
# if defined(_WIN32) && !defined(__CYGWIN__)
#  include <fast_io.h>
#  include <windows.h>
#  include <uwvm2/utils/control/win32_abi.h>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::utils::control
{
    enum class sealed_input_status { ok, already_sealed, invalid_descriptor, identity_unavailable, unsupported_platform };
    enum class sealed_input_decision { allow, denied, identity_unavailable };
    namespace details
    {
        // Mark the libc call itself nonthrowing; an enclosing noexcept alone
        // can still emit terminate landing pads when C++ exceptions are enabled.
        struct sealed_input_state
        {
            ::std::atomic_bool published{};
#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
            // Own the CLOEXEC duplicate through the complete publication and
            // guest-drain lifetime. native_handle() is only a borrowed identity.
            ::fast_io::native_file retained{};
            ::std::uintmax_t device{}, special_device{}, inode{};
            ::fast_io::file_type type{};
# if defined(__APPLE__) && defined(__MACH__)
            ::std::uint64_t pipe_handle{}, pipe_peerhandle{};
# endif
            unsigned terminal_device{};
            bool terminal{};
#elif defined(_WIN32) && !defined(__CYGWIN__)
            // This duplicate pins the management endpoint until all guest
            // activity has drained; a guest never receives this HANDLE.
            ::fast_io::native_file retained{};
            ::DWORD type{};
#endif
        };
        inline sealed_input_state sealed_console_input{};
#if defined(_WIN32) && !defined(__CYGWIN__)
        // Separate host-only capability: late debugger commands do not use
        // stdin. Keep a non-inheritable duplicate alive for exact HANDLE alias
        // checks through every guest path_open and WASI descriptor publication.
        inline sealed_input_state sealed_debug_control{};
        using compare_object_handles_fn = ::BOOL (WINAPI *)(::HANDLE, ::HANDLE) noexcept;
        [[nodiscard]] inline compare_object_handles_fn compare_object_handles() noexcept
        {
            // KernelBase exports this API on Windows 10+, but some MinGW
            // import libraries omit it. Resolve once through fast_io's
            // noexcept Win32 declarations; an unavailable API fails closed.
            static auto const function{[]() noexcept -> compare_object_handles_fn
            {
                auto const module{::fast_io::win32::GetModuleHandleW(u"KernelBase.dll")};
                if(module == nullptr) { return nullptr; }
                auto const symbol{::fast_io::win32::GetProcAddress(module, "CompareObjectHandles")};
                return reinterpret_cast<compare_object_handles_fn>(symbol);
            }()};
            return function;
        }
        [[nodiscard]] inline sealed_input_decision inspect_windows_handle(
            ::HANDLE handle, bool guest_output, bool guest_path_open = false) noexcept
        {
            auto const& sealed{sealed_console_input};
            auto const console_protected{sealed.published.load(::std::memory_order_acquire)};
            auto const control_protected{sealed_debug_control.published.load(::std::memory_order_acquire)};
            if(!console_protected && !control_protected) { return sealed_input_decision::allow; }
            if(handle == nullptr || handle == INVALID_HANDLE_VALUE) { return sealed_input_decision::identity_unavailable; }
            ::uwvm2::utils::control::win32_abi::uwvm_SetLastError(ERROR_SUCCESS);
            auto const type{::fast_io::win32::GetFileType(handle)};
            if(type == FILE_TYPE_UNKNOWN) { return sealed_input_decision::identity_unavailable; }
            if(control_protected)
            {
                auto const compare{compare_object_handles()};
                if(compare == nullptr) { return sealed_input_decision::identity_unavailable; }
                ::uwvm2::utils::control::win32_abi::uwvm_SetLastError(ERROR_SUCCESS);
                if(compare(handle, sealed_debug_control.retained.native_handle()) != 0)
                { return sealed_input_decision::denied; }
                auto const comparison_error{::fast_io::win32::GetLastError()};
                if(comparison_error != ERROR_SUCCESS && comparison_error != ERROR_NOT_SAME_OBJECT)
                { return sealed_input_decision::identity_unavailable; }
                // A single-instance control pipe cannot be reopened while its
                // launcher owns the server end. Refuse all guest path opens of
                // pipe objects as defense in depth, while allowing unrelated
                // inherited stdio pipes after exact alias comparison above.
                if(guest_path_open && type == FILE_TYPE_PIPE) { return sealed_input_decision::denied; }
                if(!console_protected)
                {
                    if(type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE) { return sealed_input_decision::allow; }
                    if(type != FILE_TYPE_CHAR) { return sealed_input_decision::identity_unavailable; }
                    if(!guest_output)
                    {
                        ::DWORD events{};
                        return ::uwvm2::utils::control::win32_abi::uwvm_GetNumberOfConsoleInputEvents(handle, ::std::addressof(events)) != 0
                            ? sealed_input_decision::allow : sealed_input_decision::identity_unavailable;
                    }
                    ::CONSOLE_SCREEN_BUFFER_INFO screen{};
                    return ::uwvm2::utils::control::win32_abi::uwvm_GetConsoleScreenBufferInfo(handle, ::std::addressof(screen)) != 0
                        ? sealed_input_decision::allow : sealed_input_decision::identity_unavailable;
                }
            }
            // The host input is restricted to a pipe or a console input buffer.
            // A disk file therefore cannot be an alias, including through a
            // hard link or renamed path. Reject all guest pipe opens, because
            // anonymous/named pipe endpoint identity is not a stable file ID.
            if(type == FILE_TYPE_DISK) { return sealed_input_decision::allow; }
            if(type == FILE_TYPE_PIPE) { return sealed_input_decision::denied; }
            if(type != FILE_TYPE_CHAR) { return sealed_input_decision::identity_unavailable; }
            if(!guest_output) { return sealed_input_decision::denied; }
            // The screen-buffer query succeeds on a console *output* handle,
            // not on CONIN$ or arbitrary character devices. Output rights are
            // separately reduced to write-only in the WASI fd table.
            ::CONSOLE_SCREEN_BUFFER_INFO screen{};
            return ::uwvm2::utils::control::win32_abi::uwvm_GetConsoleScreenBufferInfo(handle, ::std::addressof(screen)) != 0
                ? sealed_input_decision::allow : sealed_input_decision::identity_unavailable;
        }
        [[nodiscard]] inline ::HANDLE standard_windows_handle(int fd) noexcept
        {
            switch(fd)
            {
                case 0: return ::fast_io::win32::GetStdHandle(STD_INPUT_HANDLE);
                case 1: return ::fast_io::win32::GetStdHandle(STD_OUTPUT_HANDLE);
                case 2: return ::fast_io::win32::GetStdHandle(STD_ERROR_HANDLE);
                default: return nullptr;
            }
        }
#endif
#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
# if defined(__linux__)
        [[nodiscard]] inline int query_terminal_device(int fd, unsigned long request, unsigned& output) noexcept
        {
            int result{};
            do { result = ::uwvm2::utils::control::posix_abi::ioctl_noexcept(fd, request, &output); } while(result == -1 && errno == EINTR);
            // ENOTTY means this request is unavailable for the object, including
            // an unsupported ioctl under emulation; it does not prove non-TTY.
            return result == 0 ? 1 : errno == ENOTTY ? 0 : -1;
        }
        [[nodiscard]] inline int terminal_master(int fd) noexcept
        {
# if defined(TIOCGPTN)
            unsigned number{};
            return query_terminal_device(fd, TIOCGPTN, number);
# else
            static_cast<void>(fd);
            return -1;
# endif
        }
        [[nodiscard]] inline int null_character_device(::fast_io::posix_file_status const& status) noexcept
        {
            auto const queried{::fast_io::posix_fstatat_nothrow(::fast_io::posix_at_fdcwd(), "/dev/null")};
            if(!queried) { return -1; }
            auto const& known{queried.value};
            return status.type == ::fast_io::file_type::character && known.type == status.type &&
                status.dev == known.dev && status.ino == known.ino && status.rdev == known.rdev;
        }
# elif defined(__APPLE__) && defined(__MACH__)
        [[nodiscard]] inline bool query_pipe_identity(int fd, ::std::uint64_t& handle, ::std::uint64_t& peer) noexcept
        {
            ::pipe_fdinfo info{};
            auto const read{::uwvm2::utils::control::posix_abi::proc_pidfdinfo_noexcept(::uwvm2::utils::control::posix_abi::getpid_noexcept(), fd, PROC_PIDFDPIPEINFO,
                ::std::addressof(info), static_cast<int>(sizeof(info)))};
            if(read != static_cast<int>(sizeof(info)) || info.pipeinfo.pipe_handle == 0u ||
               info.pipeinfo.pipe_peerhandle == 0u ||
               info.pipeinfo.pipe_handle == info.pipeinfo.pipe_peerhandle) { return false; }
            handle = info.pipeinfo.pipe_handle;
            peer = info.pipeinfo.pipe_peerhandle;
            return true;
        }
        // TIOCPTYGNAME is accepted only on a PTY master. A successful query
        // names its live slave; ENOTTY distinguishes ordinary slave output.
        [[nodiscard]] inline int terminal_master(int fd, char (&slave_name)[128]) noexcept
        {
            int result{};
            do { result = ::uwvm2::utils::control::posix_abi::ioctl_noexcept(fd, TIOCPTYGNAME, slave_name); }
            while(result == -1 && errno == EINTR);
            return result == 0 ? 1 : errno == ENOTTY ? 0 : -1;
        }
# elif defined(__FreeBSD__)
        // FreeBSD accepts TIOCPTMASTER only on the master side. ENOTTY is
        // the sole negative identity proof; other errors fail closed.
        [[nodiscard]] inline int terminal_master(int fd) noexcept
        {
            int result{};
            do { result = ::uwvm2::utils::control::posix_abi::ioctl_noexcept(fd, TIOCPTMASTER); }
            while(result == -1 && errno == EINTR);
            return result == 0 ? 1 : errno == ENOTTY ? 0 : -1;
        }
# endif
        [[nodiscard]] inline sealed_input_decision inspect_sealed_fd(int fd, bool guest_output, ::fast_io::posix_file_status* observed = nullptr) noexcept
        {
            auto const& sealed{sealed_console_input};
            if(!sealed.published.load(::std::memory_order_acquire)) { return sealed_input_decision::allow; }
            auto const queried_status{::fast_io::posix_status_nothrow(::fast_io::posix_io_observer{fd})};
            if(!queried_status) { return sealed_input_decision::identity_unavailable; }
            // This reference borrows the complete local result only through the
            // synchronous identity check; no status cell or fd borrow escapes.
            auto const& status{queried_status.value};
            if(observed != nullptr) { *observed = status; } // Borrowed caller-owned output, valid for this call.
# if defined(__APPLE__) && defined(__MACH__)
            // An opted-in debugger can seal an anonymous Unix socket. Darwin
            // does not expose a portable socket inode identity through fstat;
            // reject every guest-visible socket while that socket is sealed.
            // This is a cold WASI descriptor-admission check, never a memory
            // access or JIT hot-path guard.
            if(sealed.type == ::fast_io::file_type::socket && (status.type == ::fast_io::file_type::socket)) { return sealed_input_decision::denied; }
# endif
            bool const same_inode{status.dev == sealed.device && status.ino == sealed.inode && status.type == sealed.type};
            bool const special{(status.type == ::fast_io::file_type::character) || (status.type == ::fast_io::file_type::block)};
            bool const same_special{special && status.type == sealed.type && status.rdev == sealed.special_device};
            bool same_terminal{};
# if defined(__APPLE__) && defined(__MACH__)
            bool same_pipe{};
            if((status.type == ::fast_io::file_type::fifo))
            {
                ::std::uint64_t handle{}, peer{};
                if(!query_pipe_identity(fd, handle, peer)) { return sealed_input_decision::identity_unavailable; }
                same_pipe = sealed.type == ::fast_io::file_type::fifo &&
                    (handle == sealed.pipe_handle || handle == sealed.pipe_peerhandle ||
                     peer == sealed.pipe_handle || peer == sealed.pipe_peerhandle);
            }
# endif
            if(sealed.terminal && (status.type == ::fast_io::file_type::character))
            {
                // A PTY master writes into its slave's input queue. Conservatively
                // reject masters while protecting a terminal, including reopened
                // /dev/fd masters; ordinary slave output has other semantics.
# if defined(__linux__)
                auto const master{terminal_master(fd)};
                if(master == 1) { return sealed_input_decision::denied; }
                if(master == -1) { return sealed_input_decision::identity_unavailable; }
# if defined(TIOCGDEV)
                unsigned device{};
                auto const queried{query_terminal_device(fd, TIOCGDEV, device)};
                if(queried == 1) { same_terminal = device == sealed.terminal_device; }
                else
                {
                    // Unknown terminal aliases cannot become guest descriptors.
                    // Preserve the exact null sink without trusting ENOTTY.
                    if(queried == 0 && null_character_device(status) == 1) { return sealed_input_decision::allow; }
                    return sealed_input_decision::identity_unavailable;
                }
# endif
# else
                if(::uwvm2::utils::control::posix_abi::isatty_noexcept(fd) == 0)
                {
                    // Darwin reports EINVAL rather than ENOTTY for some
                    // character devices. Admit only the kernel null sink by
                    // exact object identity; unknown devices fail closed.
                    auto const queried_null{::fast_io::posix_fstatat_nothrow(::fast_io::posix_at_fdcwd(), "/dev/null")};
                    if(!queried_null) { return sealed_input_decision::identity_unavailable; }
                    auto const& null_status{queried_null.value}; // Borrow the complete local result for this comparison only.
                    if(status.dev == null_status.dev && status.ino == null_status.ino &&
                       status.rdev == null_status.rdev &&
                       status.type == (null_status.type))
                    { return sealed_input_decision::allow; }
                    return sealed_input_decision::identity_unavailable;
                }
# if defined(__FreeBSD__)
                auto const master{terminal_master(fd)};
                if(master == 1) { return sealed_input_decision::denied; }
                if(master == -1) { return sealed_input_decision::identity_unavailable; }
# else
                char slave_name[128]{};
                auto const master{terminal_master(fd, slave_name)};
                if(master == -1) { return sealed_input_decision::identity_unavailable; }
                if(master == 1)
                {
                    if(::std::memchr(slave_name, 0, sizeof(slave_name)) == nullptr)
                    { return sealed_input_decision::identity_unavailable; }
                    // memchr above proves the complete owned name is NUL-terminated.
                    auto const queried_slave{::fast_io::posix_fstatat_nothrow(::fast_io::posix_at_fdcwd(), slave_name)};
                    if(!queried_slave) { return sealed_input_decision::identity_unavailable; }
                    auto const& slave_status{queried_slave.value}; // Local complete result; no pathname or metadata is retained.
                    if((slave_status.type == ::fast_io::file_type::character) && slave_status.dev == sealed.device &&
                       slave_status.ino == sealed.inode && slave_status.rdev == sealed.special_device)
                    { return sealed_input_decision::denied; }
                    // This master is bound to a different live PTY slave.
                    return sealed_input_decision::allow;
                }
# endif
                // /dev/tty can refer to the protected slave through a distinct
                // device identity. Reject any unproved terminal alias.
                if(::uwvm2::utils::control::posix_abi::isatty_noexcept(fd) != 0 && !same_inode && !same_special)
                { return sealed_input_decision::identity_unavailable; }
                same_terminal = same_inode || same_special;
# endif
            }
# if defined(__APPLE__) && defined(__MACH__)
            if(same_pipe) { return sealed_input_decision::denied; }
# endif
            if(!same_inode && !same_special && !same_terminal) { return sealed_input_decision::allow; }
            // Only the launch-owned stdout/stderr grant may retain same-terminal
            // output. Guest path_open cannot obtain any rights on that terminal.
# if defined(__linux__)
            if(guest_output && sealed.terminal && same_terminal) { return sealed_input_decision::allow; }
# else
            if(guest_output && sealed.terminal && (same_terminal || same_special)) { return sealed_input_decision::allow; }
# endif
            return sealed_input_decision::denied;
        }
#endif
    }
    [[nodiscard]] inline bool console_input_sealed() noexcept
    {
#if defined(_WIN32) && !defined(__CYGWIN__)
        return details::sealed_console_input.published.load(::std::memory_order_acquire) ||
               details::sealed_debug_control.published.load(::std::memory_order_acquire);
#else
        return details::sealed_console_input.published.load(::std::memory_order_acquire);
#endif
    }

#if defined(_WIN32) && !defined(__CYGWIN__)
    // HOST STARTUP ONLY. The caller already authenticated a connected,
    // inherited client pipe against its direct launcher parent. The duplicate
    // cannot be inherited by guest-created children and pins the exact object.
    [[nodiscard]] inline sealed_input_status seal_debug_control_handle_host_api(void* raw) noexcept
    {
        auto& sealed{details::sealed_debug_control};
        if(sealed.published.load(::std::memory_order_acquire)) { return sealed_input_status::already_sealed; }
        auto const handle{static_cast<::HANDLE>(raw)};
        if(handle == nullptr || handle == INVALID_HANDLE_VALUE || ::fast_io::win32::GetFileType(handle) != FILE_TYPE_PIPE)
        { return sealed_input_status::invalid_descriptor; }
        ::HANDLE duplicate{};
        if(::uwvm2::utils::control::win32_abi::uwvm_DuplicateHandle(::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(), handle, ::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(), ::std::addressof(duplicate),
                             0u, FALSE, DUPLICATE_SAME_ACCESS) == 0)
        { return sealed_input_status::identity_unavailable; }
        // DuplicateHandle returned exclusive ownership; reset is noexcept and
        // never exposes the duplicate to guest descriptors or child processes.
        sealed.retained.reset(duplicate);
        sealed.type = FILE_TYPE_PIPE;
        sealed.published.store(true, ::std::memory_order_release);
        return sealed_input_status::ok;
    }
#endif

    // HOST STARTUP ONLY: externally serialized before any guest/WASI environment
    // is admitted. This grant is never decoded from guest data, argv, or a wire
    // request. The retained CLOEXEC duplicate pins the exact inode against reuse.
    [[nodiscard]] inline sealed_input_status seal_console_input_host_api(int fd, bool permit_debug_socket = false) noexcept
    {
        auto& sealed{details::sealed_console_input};
        if(sealed.published.load(::std::memory_order_acquire)) { return sealed_input_status::already_sealed; }
#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
        if(fd < 0) { return sealed_input_status::invalid_descriptor; }
        ::fast_io::native_file retained{::uwvm2::utils::control::posix_abi::fcntl_noexcept(fd, F_DUPFD_CLOEXEC, 3)};
        if(retained.native_handle() == -1) { return errno == EBADF ? sealed_input_status::invalid_descriptor : sealed_input_status::identity_unavailable; }
        auto const queried_status{::fast_io::posix_status_nothrow(::fast_io::posix_io_observer{retained.native_handle()})};
        if(!queried_status || queried_status.value.type == ::fast_io::file_type::directory) { return sealed_input_status::identity_unavailable; }
        auto const& status{queried_status.value}; // Borrow complete local metadata while retained pins the same file.
        // Borrow only the retained owner for one readonly flag query. Zero
        // status flags are valid; the fast_io error field decides failure.
        auto const queried_flags{::fast_io::posix_getfl_nothrow(::fast_io::posix_io_observer{retained.native_handle()})};
        if(!queried_flags || (queried_flags.flags & O_ACCMODE) == O_WRONLY) { return sealed_input_status::invalid_descriptor; }
# if defined(O_PATH)
        if((queried_flags.flags & O_PATH) != 0) { return sealed_input_status::invalid_descriptor; }
# endif
        bool terminal{};
        unsigned terminal_device{};
# if defined(__APPLE__) && defined(__MACH__)
        ::std::uint64_t pipe_handle{}, pipe_peerhandle{};
# elif defined(__FreeBSD__)
        static_cast<void>(permit_debug_socket);
# endif
        if((status.type == ::fast_io::file_type::character))
        {
# if defined(__APPLE__) && defined(__MACH__)
            if(::uwvm2::utils::control::posix_abi::isatty_noexcept(retained.native_handle()) == 0)
            { return sealed_input_status::invalid_descriptor; }
            char slave_name[128]{};
            if(details::terminal_master(retained.native_handle(), slave_name) != 0)
            { return sealed_input_status::identity_unavailable; }
            terminal = true;
# elif defined(__FreeBSD__)
            if(::uwvm2::utils::control::posix_abi::isatty_noexcept(retained.native_handle()) == 0)
            { return sealed_input_status::invalid_descriptor; }
            if(details::terminal_master(retained.native_handle()) != 0)
            { return sealed_input_status::identity_unavailable; }
            terminal = true;
# elif defined(TIOCGDEV) && defined(TIOCGPTN)
            // Probe masters independently: emulators can support TIOCGPTN
            // while rejecting TIOCGDEV. Such a master must never be published.
            if(details::terminal_master(retained.native_handle()) != 0)
            { return sealed_input_status::identity_unavailable; }
            auto const queried{details::query_terminal_device(retained.native_handle(), TIOCGDEV, terminal_device)};
            if(queried == -1) { return sealed_input_status::identity_unavailable; }
            terminal = queried == 1;
            if(!terminal && details::null_character_device(status) != 1)
            { return sealed_input_status::identity_unavailable; }
# else
            // Without a reliable alias query, character input cannot be sealed.
            return sealed_input_status::unsupported_platform;
# endif
        }
# if defined(__APPLE__) && defined(__MACH__)
        else if((status.type == ::fast_io::file_type::fifo))
        {
            if(!details::query_pipe_identity(retained.native_handle(), pipe_handle, pipe_peerhandle))
            { return sealed_input_status::identity_unavailable; }
        }
        else if((status.type == ::fast_io::file_type::socket) && permit_debug_socket)
        {
            // Only the macOS debug-control adopter may request this branch,
            // after checking peer credentials and unnamed AF_UNIX SOCK_STREAM.
            // Ordinary -m debug-jit console input still rejects sockets.
        }
        else if(!(status.type == ::fast_io::file_type::regular)) { return sealed_input_status::unsupported_platform; }
# elif defined(__FreeBSD__)
        // No cancellable independently owned reader for a FreeBSD anonymous
        // pipe is installed. Do not admit a shared blocking descriptor.
        else if(status.type != ::fast_io::file_type::regular)
        { return sealed_input_status::unsupported_platform; }
# elif defined(__linux__)
        // Socket peers have distinct inode identities. A guest output peer can
        // feed an ordinary console even though it is not an input FD alias.
        // Only the authenticated host-startup control adopter opts in.
        else if(status.type == ::fast_io::file_type::socket && !permit_debug_socket)
        { return sealed_input_status::unsupported_platform; }
# endif
        sealed.device = status.dev; sealed.inode = status.ino; sealed.type = status.type;
        sealed.special_device = status.rdev; sealed.terminal = terminal; sealed.terminal_device = terminal_device;
# if defined(__APPLE__) && defined(__MACH__)
        sealed.pipe_handle = pipe_handle; sealed.pipe_peerhandle = pipe_peerhandle;
# endif
        // The verified local owner transfers once; no fallible operation or
        // guest entry occurs before the release-store publishes its identity.
        sealed.retained = ::std::move(retained);
        sealed.published.store(true, ::std::memory_order_release);
        return sealed_input_status::ok;
#elif defined(_WIN32) && !defined(__CYGWIN__)
        static_cast<void>(permit_debug_socket); // Windows debug control uses the separate authenticated HANDLE API.
        if(fd != 0) { return sealed_input_status::invalid_descriptor; }
        auto const input{details::standard_windows_handle(fd)};
        if(input == nullptr || input == INVALID_HANDLE_VALUE) { return sealed_input_status::invalid_descriptor; }
        ::uwvm2::utils::control::win32_abi::uwvm_SetLastError(ERROR_SUCCESS);
        auto const type{::fast_io::win32::GetFileType(input)};
        if(type == FILE_TYPE_PIPE)
        {
            ::DWORD flags{};
            if(::uwvm2::utils::control::win32_abi::uwvm_GetNamedPipeInfo(input, ::std::addressof(flags), nullptr, nullptr, nullptr) == 0)
            { return sealed_input_status::identity_unavailable; }
        }
        else if(type == FILE_TYPE_CHAR)
        {
            ::DWORD events{};
            if(::uwvm2::utils::control::win32_abi::uwvm_GetNumberOfConsoleInputEvents(input, ::std::addressof(events)) == 0)
            { return sealed_input_status::identity_unavailable; }
        }
        else { return sealed_input_status::unsupported_platform; }
        ::HANDLE duplicate{};
        if(::uwvm2::utils::control::win32_abi::uwvm_DuplicateHandle(::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(), input, ::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(), ::std::addressof(duplicate),
                             0u, FALSE, DUPLICATE_SAME_ACCESS) == 0)
        { return sealed_input_status::identity_unavailable; }
        // DuplicateHandle returned exclusive ownership; reset is noexcept and
        // never exposes the duplicate to guest descriptors or child processes.
        sealed.retained.reset(duplicate);
        sealed.type = type;
        sealed.published.store(true, ::std::memory_order_release);
        return sealed_input_status::ok;
#else
        static_cast<void>(fd);
        return sealed_input_status::unsupported_platform;
#endif
    }
    [[nodiscard]] inline sealed_input_decision inspect_guest_file_host_api(int fd) noexcept
    {
#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
        return details::inspect_sealed_fd(fd, false);
#elif defined(_WIN32) && !defined(__CYGWIN__)
        return details::inspect_windows_handle(details::standard_windows_handle(fd), false);
#else
        static_cast<void>(fd);
        return console_input_sealed() ? sealed_input_decision::identity_unavailable : sealed_input_decision::allow;
#endif
    }
    [[nodiscard]] inline sealed_input_decision inspect_guest_output_host_api(int fd) noexcept
    {
#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
        return details::inspect_sealed_fd(fd, true);
#elif defined(_WIN32) && !defined(__CYGWIN__)
        return details::inspect_windows_handle(details::standard_windows_handle(fd), true);
#else
        static_cast<void>(fd);
        return console_input_sealed() ? sealed_input_decision::identity_unavailable : sealed_input_decision::allow;
#endif
    }
#if defined(_WIN32) && !defined(__CYGWIN__)
    // Inspect the opened kernel object before a guest-visible handle is
    // published. In debug mode, pipe and character devices never enter WASI.
    [[nodiscard]] inline sealed_input_decision inspect_guest_native_handle_host_api(void* handle) noexcept
    { return details::inspect_windows_handle(static_cast<::HANDLE>(handle), false, true); }
#endif
    // HOST ONLY, AFTER stopping all new guest entries and draining all admitted
    // operations. No per-file/open reader lease is added to ordinary execution.
    // Clearing while a guest call is live would violate this ownership contract.
    inline void unseal_console_input_after_guest_drain_host_api() noexcept
    {
        auto& sealed{details::sealed_console_input};
        sealed.published.store(false, ::std::memory_order_release);
#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
        // Invalidate the persistent owner before one nonthrowing RAII retire;
        // all guest operations have drained, so no identity borrow remains.
        ::fast_io::native_file retired{sealed.retained.release()};
#elif defined(_WIN32) && !defined(__CYGWIN__)
        ::fast_io::native_file retired{sealed.retained.release()};
#endif
    }
    struct sealed_open_result { int descriptor{-1}, error{}; };
#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
    // pathname borrows the caller's validated, NUL-terminated host copy through
    // this synchronous open only. The returned fd is unpublished and exclusively
    // owned until all identity checks and deferred destructive actions finish.
    [[nodiscard]] inline sealed_open_result open_guest_path_sealed_host_api(
        int directory, char const* pathname, int flags, unsigned permissions) noexcept
    {
        if(!console_input_sealed()) { return {-1, EPERM}; } // Caller must explicitly opt in.
        bool const truncate{(flags & O_TRUNC) != 0};
        auto opened_result{::fast_io::posix_openat_nothrow(::fast_io::posix_at_entry{directory}, pathname,
            (flags & ~O_TRUNC) | O_CLOEXEC | O_NOCTTY, static_cast<::fast_io::perms>(permissions))};
        if(!opened_result) { return {-1, opened_result.error}; }
        // The move-only result exclusively owns this unpublished descriptor.
        // Borrow its file only until authorization/deferred truncation finishes.
        auto& opened{opened_result.file};
        ::fast_io::posix_file_status status{};
        auto const decision{details::inspect_sealed_fd(opened.native_handle(), false, &status)};
        if(decision != sealed_input_decision::allow)
        { return {-1, decision == sealed_input_decision::denied ? EACCES : EIO}; }
        // Reopening an input regular file with O_TRUNC must not destroy commands
        // before authorization. Perform it only now, on this same retained fd.
        if(truncate && status.type == ::fast_io::file_type::regular)
        {
            auto const result{::fast_io::posix_truncate_nothrow(::fast_io::posix_io_observer{opened.native_handle()}, 0)};
            if(!result) { return {-1, result.error}; }
        }
        return {opened.release(), 0};
    }
#endif
}
