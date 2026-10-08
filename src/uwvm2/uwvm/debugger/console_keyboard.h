/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <memory>
# include <string>
# include <utility>
# include <fast_io.h>
# include <fast_io_unit/string.h>
# include "console_line_editor.h"
# if defined(__unix__) || defined(__APPLE__)
#  include <cerrno>
#  include <poll.h>
#  include <signal.h>
#  include <termios.h>
#  include <unistd.h>
# elif defined(_WIN32) && !defined(__CYGWIN__)
#  include <windows.h>
#  include <uwvm2/utils/control/win32_abi.h>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_keyboard
{
    inline constexpr int interrupted{-3};
    // State is process-local HOST management infrastructure, never a Wasm
    // import, signal-to-capability conversion or native-memory read credential.
    namespace details
    {
        static_assert(::std::atomic<unsigned>::is_always_lock_free);
        inline ::std::atomic<unsigned> active{}, pending{}, process_id{}, owner_uid{}, terminal_owner{};
#if defined(__unix__) || defined(__APPLE__)
# if defined(__APPLE__)
#  define UWVM_KEYBOARD_POSIX_SYMBOL(name) __asm__("_" #name)
# else
#  define UWVM_KEYBOARD_POSIX_SYMBOL(name) __asm__(#name)
# endif
        extern "C" int sigaction_noexcept(int, struct ::sigaction const*, struct ::sigaction*) noexcept UWVM_KEYBOARD_POSIX_SYMBOL(sigaction);
        extern "C" int sigemptyset_noexcept(::sigset_t*) noexcept UWVM_KEYBOARD_POSIX_SYMBOL(sigemptyset);
        extern "C" int poll_noexcept(::pollfd*, ::nfds_t, int) noexcept UWVM_KEYBOARD_POSIX_SYMBOL(poll);
        extern "C" int tcgetattr_noexcept(int, ::termios*) noexcept UWVM_KEYBOARD_POSIX_SYMBOL(tcgetattr);
        extern "C" int tcsetattr_noexcept(int, int, ::termios const*) noexcept UWVM_KEYBOARD_POSIX_SYMBOL(tcsetattr);
        extern "C" int tcflush_noexcept(int, int) noexcept UWVM_KEYBOARD_POSIX_SYMBOL(tcflush);
        extern "C" ::pid_t getpid_noexcept() noexcept UWVM_KEYBOARD_POSIX_SYMBOL(getpid);
        extern "C" ::uid_t geteuid_noexcept() noexcept UWVM_KEYBOARD_POSIX_SYMBOL(geteuid);
        extern "C" int raise_noexcept(int) noexcept UWVM_KEYBOARD_POSIX_SYMBOL(raise);
        extern "C" void exit_noexcept(int) noexcept UWVM_KEYBOARD_POSIX_SYMBOL(_exit);
# undef UWVM_KEYBOARD_POSIX_SYMBOL
        // The callback never borrows a session object or non-atomic static POD.
        // One native CLI installation per process prevents old asynchronous
        // callbacks from racing a later rewrite of the prior disposition.
        using signal_handler = void (*)(int);
        using info_handler = void (*)(int, ::siginfo_t*, void*);
        static_assert(::std::atomic<signal_handler>::is_always_lock_free);
        static_assert(::std::atomic<info_handler>::is_always_lock_free);
        inline ::std::atomic<signal_handler> prior_handler{};
        inline ::std::atomic<info_handler> prior_info{};
        inline ::std::atomic<unsigned> prior_flags{}, prior_kind{}, prior_reset_claim{};
        inline void forward_interrupt(int number, ::siginfo_t* info, void* context) noexcept
        {
            auto const saved_errno{errno};
            auto const flags{prior_flags.load(::std::memory_order_acquire)};
            auto kind{prior_kind.load(::std::memory_order_relaxed)};
            if(kind == 1u) { errno = saved_errno; return; } // predecessor SIG_IGN
            if((flags & SA_RESETHAND) != 0u && prior_reset_claim.exchange(1u, ::std::memory_order_relaxed) != 0u) { kind = 0u; }
            if(kind == 0u || (flags & SA_RESETHAND) != 0u)
            {
                // sigaction/sigemptyset/raise are POSIX async-signal-safe.
                // Restore the default BEFORE redelivery; it cannot recurse
                // into this callback. Guest proc_raise keeps its disposition.
                struct ::sigaction reset{}; reset.sa_handler = SIG_DFL;
                static_cast<void>(sigemptyset_noexcept(::std::addressof(reset.sa_mask)));
                if(sigaction_noexcept(number, ::std::addressof(reset), nullptr) != 0)
                { exit_noexcept(128 + number); } // async-safe fatal fallback; never recursive redelivery
                active.store(3u, ::std::memory_order_release);
                if(kind == 0u)
                { static_cast<void>(raise_noexcept(number)); errno = saved_errno; return; }
            }
            if(kind == 3u)
            { if(auto const prior{prior_info.load(::std::memory_order_relaxed)}; prior != nullptr) { ::fast_io::noexcept_call(prior, number, info, context); } }
            else if(auto const prior{prior_handler.load(::std::memory_order_relaxed)}; prior != nullptr) { ::fast_io::noexcept_call(prior, number); }
            errno = saved_errno;
        }
        inline void interrupt_handler(int number, ::siginfo_t* info, void* context) noexcept
        {
            // The kernel owns this bounded siginfo record through this callback.
            // Accepted interrupts do only lock-free scalar operations. Rejected
            // guest signals forward only the prior handler or POSIX async-safe
            // disposition operations; no printing/allocation/controller access.
            if(number != SIGINT || info == nullptr) { return; }
            if(active.load(::std::memory_order_acquire) != 1u) { forward_interrupt(number, info, context); return; }
            auto const saved_errno{errno};
            bool terminal{};
# if defined(SI_KERNEL)
            terminal = details::terminal_owner.load(::std::memory_order_relaxed) != 0u && info->si_code == SI_KERNEL;
# endif
            # if !defined(__linux__)
            // BSD/macOS kernels report terminal signals with sender PID zero.
            // This branch still requires a successfully reserved terminal.
            terminal = terminal || (terminal_owner.load(::std::memory_order_relaxed) != 0u && info->si_pid == 0);
# endif
            // Guest proc_raise/native reentry may raise SIGINT in this process.
            // Reject self-originated signals. External host-owner signals and
            // genuine terminal signals can only REQUEST an ordinary pause; all
            // stopped/source/native permissions still require fresh real captures.
            bool const external{info->si_pid > 0 && static_cast<unsigned>(info->si_pid) != process_id.load(::std::memory_order_relaxed) &&
                static_cast<unsigned>(info->si_uid) == owner_uid.load(::std::memory_order_relaxed)};
            if(terminal || external) { pending.store(1u, ::std::memory_order_relaxed); }
            else { forward_interrupt(number, info, context); }
            errno = saved_errno;
        }
#elif defined(_WIN32) && !defined(__CYGWIN__)
        // Match fast_io's nonthrowing, SDK-typed stdcall import spelling.
# if defined(__GNUC__) || defined(__clang__)
#  if defined(_M_HYBRID)
#   define UWVM_KEYBOARD_WIN_SYMBOL(name, count) __asm__("#" #name "@" #count)
#  elif defined(__arm64ec__) || defined(_M_ARM64EC)
#   define UWVM_KEYBOARD_WIN_SYMBOL(name, count) __asm__("#" #name)
#  elif defined(__i386__) || defined(_M_IX86)
#   if defined(__clang__)
#    define UWVM_KEYBOARD_WIN_SYMBOL(name, count) __asm__("_" #name "@" #count)
#   else
#    define UWVM_KEYBOARD_WIN_SYMBOL(name, count) __asm__(#name "@" #count)
#   endif
#  else
#   define UWVM_KEYBOARD_WIN_SYMBOL(name, count) __asm__(#name)
#  endif
        extern "C" __declspec(dllimport) ::BOOL WINAPI set_console_handler_noexcept(::PHANDLER_ROUTINE, ::BOOL) noexcept UWVM_KEYBOARD_WIN_SYMBOL(SetConsoleCtrlHandler, 8);
        extern "C" __declspec(dllimport) ::BOOL WINAPI set_console_mode_noexcept(::HANDLE, ::DWORD) noexcept UWVM_KEYBOARD_WIN_SYMBOL(SetConsoleMode, 8);
        extern "C" __declspec(dllimport) ::BOOL WINAPI flush_console_input_noexcept(::HANDLE) noexcept UWVM_KEYBOARD_WIN_SYMBOL(FlushConsoleInputBuffer, 4);
        extern "C" __declspec(dllimport) ::BOOL WINAPI read_console_events_noexcept(::HANDLE, ::PINPUT_RECORD, ::DWORD, ::LPDWORD) noexcept UWVM_KEYBOARD_WIN_SYMBOL(ReadConsoleInputW, 16);
#  undef UWVM_KEYBOARD_WIN_SYMBOL
# else
        inline ::BOOL set_console_handler_noexcept(::PHANDLER_ROUTINE f, ::BOOL add) noexcept
        { return ::fast_io::noexcept_call(::SetConsoleCtrlHandler, f, add); }
        inline ::BOOL set_console_mode_noexcept(::HANDLE h, ::DWORD m) noexcept
        { return ::fast_io::noexcept_call(::SetConsoleMode, h, m); }
        inline ::BOOL flush_console_input_noexcept(::HANDLE h) noexcept
        { return ::fast_io::noexcept_call(::FlushConsoleInputBuffer, h); }
        inline ::BOOL read_console_events_noexcept(::HANDLE h, ::PINPUT_RECORD p, ::DWORD n, ::LPDWORD got) noexcept
        { return ::fast_io::noexcept_call(::ReadConsoleInputW, h, p, n, got); }
# endif
        using read_console_nowait_fn = ::BOOL (WINAPI *)(::HANDLE, ::PINPUT_RECORD, ::DWORD, ::LPDWORD, ::USHORT) noexcept;
        inline constexpr ::USHORT console_read_nowait{0x0002u};
        struct pipe_read_state final
        {
            ::HANDLE input{};    // borrowed: native_session::input_owner_ outlives worker join
            ::HANDLE entered{};  // borrowed: manager's native_file outlives worker join
            ::std::atomic<::DWORD> error{ERROR_IO_PENDING};
            ::std::atomic<::DWORD> count{};
            ::std::atomic<unsigned> byte{};
            ::std::atomic<::DWORD> worker_error{};
        };
        inline ::DWORD WINAPI pipe_read_worker(void* raw) noexcept
        {
            auto& state{*static_cast<pipe_read_state*>(raw)};
            if(::uwvm2::utils::control::win32_abi::uwvm_SetEvent(state.entered) == 0)
            { state.worker_error.store(::fast_io::win32::GetLastError(), ::std::memory_order_release); return 1u; }
            char byte{};
            // [byte, byte+1) belongs only to this worker until FastIO's
            // synchronous ReadFile boundary returns. The manager cannot read
            // state or retire the borrowed input/event owners until the
            // worker thread's kernel completion ACK has been observed.
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
                auto const next{::fast_io::operations::read_some(
                    ::fast_io::win32_io_observer{state.input},
                    ::std::addressof(byte), ::std::addressof(byte) + 1u)};
                state.byte.store(static_cast<unsigned char>(byte), ::std::memory_order_relaxed);
                state.count.store(static_cast<::DWORD>(next - ::std::addressof(byte)), ::std::memory_order_relaxed);
                state.error.store(ERROR_SUCCESS, ::std::memory_order_release);
                return 0u;
            }
            catch(::fast_io::error const& ex)
            {
                if(!::fast_io::is_domain<::fast_io::win32_code>(ex))
                { state.worker_error.store(ERROR_INVALID_FUNCTION, ::std::memory_order_release); return 2u; }
                state.error.store(static_cast<::DWORD>(ex.code), ::std::memory_order_release);
                return 0u;
            }
            catch(...)
            { state.worker_error.store(ERROR_INVALID_FUNCTION, ::std::memory_order_release); return 3u; }
#else
            state.worker_error.store(ERROR_NOT_SUPPORTED, ::std::memory_order_release);
            return 4u;
#endif
        }
        [[noreturn]] inline void pipe_read_fatal() noexcept
        {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
                ::fast_io::io::perr(::fast_io::err(),
                    "fatal: Windows debugger pipe input could not be canceled and joined safely\n");
            }
            catch(...) {}
#endif
            // A still-pending synchronous ReadFile owns its stack state and
            // byte buffer. Exiting is the only safe bounded fail-closed path;
            // never TerminateThread or destroy/reuse those owners first.
            ::fast_io::win32::ExitProcess(1u);
            ::std::unreachable();
        }
        inline ::BOOL WINAPI interrupt_handler(::DWORD event) noexcept
        {
            // Console callback thread only signals lock-free static storage.
            // It owns no input HANDLE, VM pointer, allocation or controller lock.
            if((event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT) || active.load(::std::memory_order_acquire) != 1u) { return FALSE; }
            pending.store(1u, ::std::memory_order_relaxed); return TRUE;
        }
#endif
    }
#if defined(__linux__)
    namespace details
    {
        // Management-only factory. The source observer is pinned by a private
        // native_file duplicate before this call; its pathname cannot be reused
        // by a guest FD table. A /proc fresh open creates a DIFFERENT open-file
        // description. O_NONBLOCK never mutates the launcher's shared flags.
        [[nodiscard]] inline ::fast_io::posix_openat_result independent_fifo_input(::fast_io::posix_io_observer source) noexcept
        {
            auto const original{::fast_io::posix_status_nothrow(source)};
            auto const original_flags{::fast_io::posix_getfl_nothrow(source)};
            if(!original || !original_flags || original.value.type != ::fast_io::file_type::fifo ||
               (original_flags.flags & O_ACCMODE) == O_WRONLY)
            { return {::fast_io::posix_file{}, EINVAL}; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
                auto const path{::fast_io::concat_std("/proc/self/fd/", ::fast_io::mnp::dec(source.native_handle()))};
                auto opened{::fast_io::posix_openat_nothrow(::fast_io::posix_at_entry{AT_FDCWD}, path.c_str(),
                    O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOCTTY)};
                if(!opened) { return opened; }
                auto const actual{::fast_io::posix_status_nothrow(::fast_io::posix_io_observer{opened.file.native_handle()})};
                auto const actual_flags{::fast_io::posix_getfl_nothrow(::fast_io::posix_io_observer{opened.file.native_handle()})};
                auto const retained_flags{::fast_io::posix_getfl_nothrow(source)};
                if(!actual || !actual_flags || !retained_flags || actual.value.type != ::fast_io::file_type::fifo ||
                   actual.value.dev != original.value.dev || actual.value.ino != original.value.ino ||
                   (actual_flags.flags & O_ACCMODE) != O_RDONLY || (actual_flags.flags & O_NONBLOCK) == 0 ||
                   retained_flags.flags != original_flags.flags)
                { return {::fast_io::posix_file{}, EIO}; }
                return opened; // one owned reader, closed by native_file RAII on every failure/exit
            }
            catch(...) { return {::fast_io::posix_file{}, ENOMEM}; }
#else
            return {::fast_io::posix_file{}, ENOTSUP};
#endif
        }
    }
#endif
    class native_session final
    {
        bool installed_{}, interactive_{}, interactive_display_{};
#if defined(__unix__) || defined(__APPLE__)
        ::fast_io::native_file input_owner_{};
        int input_{-1}; // only borrowed while input_owner_ pins the kernel object
        struct ::sigaction previous_{};
        ::termios terminal_{};
#elif defined(_WIN32) && !defined(__CYGWIN__)
        ::fast_io::native_file input_owner_{};
        ::HANDLE input_{}; // borrowed only while the non-inheritable private owner pins it
        ::fast_io::native_dll_file console_module_{};
        details::read_console_nowait_fn read_console_nowait_{};
        ::std::uint_least32_t terminal_mode_{};
        ::std::uint_least32_t input_type_{};
        ::HANDLE output_{}; ::std::uint_least32_t output_mode_{};
        bool output_mode_changed_{};
        char8_t queued_[4]{};
        unsigned queued_size_{}, queued_at_{}, repeat_count_{};
        int repeat_key_{};
        char16_t high_surrogate_{};
#endif
#if defined(_WIN32) && !defined(__CYGWIN__)
        [[nodiscard]] int read_pipe_byte() noexcept
        {
            if(!ready() || !input_owner_) { return -2; }
            ::fast_io::native_file entered{
                ::uwvm2::utils::control::win32_abi::uwvm_CreateEventW(nullptr, TRUE, FALSE, nullptr)};
            if(!entered) { return -2; }
            details::pipe_read_state state{};
            state.input = input_; state.entered = static_cast<::HANDLE>(entered.native_handle());
            ::DWORD worker_id{};
            ::fast_io::native_file worker{::uwvm2::utils::control::win32_abi::uwvm_CreateThread(
                nullptr, 0u, details::pipe_read_worker, ::std::addressof(state), 0u, ::std::addressof(worker_id))};
            if(!worker) { return -2; }
            // CancelSynchronousIo requires THREAD_TERMINATE. This private
            // non-inheritable token and the original worker owner remain live
            // until the thread-completion ACK; neither is a guest capability.
            ::HANDLE cancel_raw{};
            if(::uwvm2::utils::control::win32_abi::uwvm_DuplicateHandle(
                ::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(),
                static_cast<::HANDLE>(worker.native_handle()),
                ::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(),
                ::std::addressof(cancel_raw), THREAD_TERMINATE | SYNCHRONIZE,
                FALSE, 0u) == 0) { details::pipe_read_fatal(); }
            ::fast_io::native_file cancel_token{cancel_raw};
            ::DWORD flags{};
            if(::uwvm2::utils::control::win32_abi::uwvm_GetHandleInformation( cancel_raw, ::std::addressof(flags)) == 0 ||
               (flags & HANDLE_FLAG_INHERIT) != 0u) { details::pipe_read_fatal(); }
            bool interrupted_request{}, retired_request{}, stale_read{};
            auto const read_started{::uwvm2::utils::control::win32_abi::uwvm_GetTickCount64()};
            for(;;)
            {
                if(consume_interrupt()) { interrupted_request = true; break; }
                if(!ready()) { retired_request = true; break; }
                auto const wait{::fast_io::win32::WaitForSingleObject(worker.native_handle(), 50u)};
                if(wait == WAIT_OBJECT_0) { break; }
                if(wait != WAIT_TIMEOUT) { details::pipe_read_fatal(); }
                if(::uwvm2::utils::control::win32_abi::uwvm_GetTickCount64() - read_started > 250u)
                { stale_read = true; break; }
            }
            if(interrupted_request || retired_request || stale_read)
            {
                auto const started{::uwvm2::utils::control::win32_abi::uwvm_GetTickCount64()};
                for(;;)
                {
                    auto const complete{::fast_io::win32::WaitForSingleObject(worker.native_handle(), 0u)};
                    if(complete == WAIT_OBJECT_0) { break; }
                    if(complete != WAIT_TIMEOUT) { details::pipe_read_fatal(); }
                    // The entered event is only a pre-ReadFile witness. A
                    // cancel-before-read race may return ERROR_NOT_FOUND;
                    // retry the actual kernel request until the worker exits
                    // or the finite deadline expires. A successful request
                    // does NOT authorize freeing its state or input owner.
                    auto const requested{::uwvm2::utils::control::win32_abi::uwvm_CancelSynchronousIo(
                        static_cast<::HANDLE>(cancel_token.native_handle()))};
                    if(requested == 0 && ::fast_io::win32::GetLastError() != ERROR_NOT_FOUND)
                    { details::pipe_read_fatal(); }
                    auto const acknowledged{::fast_io::win32::WaitForSingleObject(worker.native_handle(), 25u)};
                    if(acknowledged == WAIT_OBJECT_0) { break; }
                    if(acknowledged != WAIT_TIMEOUT ||
                       ::uwvm2::utils::control::win32_abi::uwvm_GetTickCount64() - started > 2000u)
                    { details::pipe_read_fatal(); }
                }
            }
            ::DWORD exit_code{};
            if(::uwvm2::utils::control::win32_abi::uwvm_GetExitCodeThread(
                static_cast<::HANDLE>(worker.native_handle()), ::std::addressof(exit_code)) == 0 ||
               exit_code != 0u || state.worker_error.load(::std::memory_order_acquire) != 0u)
            { return -2; }
            // The worker has exited: now its local byte and all borrowed
            // HANDLE values are quiescent. Atomics copy only initialized
            // status/count/byte; no stack pointer survives this return.
            if(interrupted_request || consume_interrupt()) { return interrupted; }
            if(retired_request || !ready()) { return -2; }
            auto const code{state.error.load(::std::memory_order_acquire)};
            auto const count{state.count.load(::std::memory_order_acquire)};
            if(code == ERROR_BROKEN_PIPE || (code == ERROR_SUCCESS && count == 0u)) { return -1; }
            if(stale_read && code == ERROR_OPERATION_ABORTED) { return -4; } // private retry after stolen readiness
            if(code != ERROR_SUCCESS || count != 1u) { return -2; }
            return static_cast<int>(state.byte.load(::std::memory_order_acquire));
        }
#endif
    public:
        native_session() noexcept
        {
            unsigned expected{};
            if(!details::active.compare_exchange_strong(expected, 2u, ::std::memory_order_acq_rel)) { return; }
            details::pending.store(0u, ::std::memory_order_relaxed);
            details::terminal_owner.store(0u, ::std::memory_order_relaxed);
#if defined(__unix__) || defined(__APPLE__)
            bool terminal_required{};
# if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#  if defined(F_DUPFD_CLOEXEC)
                // FastIO's existing typed/syscall fcntl adapter creates the
                // CLOEXEC duplicate atomically. io_dup would inherit across an
                // exec before a later F_SETFD, so it is unsuitable here.
                input_owner_ = ::fast_io::native_file{::fast_io::details::sys_fcntl(
                    ::fast_io::in().native_handle(), F_DUPFD_CLOEXEC, 3)};
                input_ = input_owner_.native_handle();
#  else
                details::active.store(0u, ::std::memory_order_release); return;
#  endif
#  if defined(__linux__)
                auto const identity{::fast_io::posix_status_nothrow(::fast_io::posix_io_observer{input_})};
                if(!identity) { details::active.store(0u, ::std::memory_order_release); return; }
                if(identity.value.type == ::fast_io::file_type::fifo)
                {
                    auto fresh{details::independent_fifo_input(::fast_io::posix_io_observer{input_})};
                    if(!fresh) { details::active.store(0u, ::std::memory_order_release); return; }
                    input_owner_ = ::std::move(fresh.file); input_ = input_owner_.native_handle();
                }
                else if(identity.value.type == ::fast_io::file_type::character)
                {
                    // Arbitrary character devices have no bounded-read contract.
                    // Only an actual terminal qualifies for VMIN/VTIME handling.
                    if(details::tcgetattr_noexcept(input_, ::std::addressof(terminal_)) != 0)
                    { details::active.store(0u, ::std::memory_order_release); return; }
                    terminal_required = true;
                }
                else if(identity.value.type != ::fast_io::file_type::regular)
                { details::active.store(0u, ::std::memory_order_release); return; }
#  elif defined(__APPLE__) || defined(__FreeBSD__)
                auto const identity{::fast_io::posix_status_nothrow(::fast_io::posix_io_observer{input_})};
                if(!identity) { details::active.store(0u, ::std::memory_order_release); return; }
                if(identity.value.type == ::fast_io::file_type::character)
                {
                    // Only a real terminal has the existing bounded VMIN/VTIME
                    // contract. dup/F_DUPFD_CLOEXEC shares an open-file description;
                    // poll readiness alone cannot make a synchronous pipe read safe.
                    if(details::tcgetattr_noexcept(input_, ::std::addressof(terminal_)) != 0)
                    { details::active.store(0u, ::std::memory_order_release); return; }
                    terminal_required = true;
                }
                else if(identity.value.type != ::fast_io::file_type::regular)
                {
                    // No independently owned nonblocking/cancellable pipe reader
                    // is qualified on these targets yet. Reject before installing
                    // SIGINT or changing any input flags/terminal modes. The guest's
                    // original stdin/OFD and its signal disposition stay unchanged.
                    details::active.store(0u, ::std::memory_order_release); return;
                }
#  else
                // The generic POSIX read_byte branch has no qualified transfer
                // provider. Do not publish ready and spin/block on poll alone.
                details::active.store(0u, ::std::memory_order_release); return;
#  endif
            }
            catch(...) { details::active.store(0u, ::std::memory_order_release); return; }
# else
            details::active.store(0u, ::std::memory_order_release); return;
# endif
            details::process_id.store(static_cast<unsigned>(details::getpid_noexcept()), ::std::memory_order_relaxed);
            details::owner_uid.store(static_cast<unsigned>(details::geteuid_noexcept()), ::std::memory_order_relaxed);
            if(details::sigaction_noexcept(SIGINT, nullptr, ::std::addressof(previous_)) != 0 ||
               ((previous_.sa_flags & SA_SIGINFO) != 0 && previous_.sa_sigaction == details::interrupt_handler))
            { details::active.store(0u, ::std::memory_order_release); return; }
            // Keep the predecessor's mask, alternate-stack and restart/nodefer
            // behavior when a rejected guest signal is forwarded. RESETHAND is
            // enacted only on a real forwarded delivery, not on keyboard input.
            auto action{previous_}; action.sa_sigaction = details::interrupt_handler;
            action.sa_flags = (previous_.sa_flags & ~SA_RESETHAND) | SA_SIGINFO;
            if((previous_.sa_flags & SA_SIGINFO) != 0)
            {
                auto const predecessor{previous_.sa_sigaction};
                auto const kind{predecessor == reinterpret_cast<details::info_handler>(reinterpret_cast<void*>(SIG_DFL)) ? 0u :
                    predecessor == reinterpret_cast<details::info_handler>(reinterpret_cast<void*>(SIG_IGN)) ? 1u : 3u};
                details::prior_info.store(predecessor, ::std::memory_order_relaxed);
                details::prior_kind.store(kind, ::std::memory_order_relaxed);
            }
            else
            {
                auto const predecessor{previous_.sa_handler};
                details::prior_handler.store(predecessor, ::std::memory_order_relaxed);
                details::prior_kind.store(predecessor == SIG_DFL ? 0u : predecessor == SIG_IGN ? 1u : 2u, ::std::memory_order_relaxed);
            }
            details::prior_flags.store(static_cast<unsigned>(previous_.sa_flags), ::std::memory_order_release);
            details::active.store(2u, ::std::memory_order_release); // publish immutable predecessor before install
            if(details::sigaction_noexcept(SIGINT, ::std::addressof(action), nullptr) != 0)
            { details::active.store(0u, ::std::memory_order_release); return; }
            installed_ = true; // owns restoration even if setup/ready publication fails
            if(details::tcgetattr_noexcept(input_, ::std::addressof(terminal_)) == 0)
            {
                auto raw{terminal_};
                // IXON consumes Ctrl+S as STOP before the owned editor can
                // read it. The saved complete termios is restored by RAII.
                raw.c_iflag &= static_cast<::tcflag_t>(~IXON);
                raw.c_lflag &= static_cast<::tcflag_t>(~(ICANON | ECHO | ECHONL | NOFLSH));
                raw.c_lflag |= ISIG;
                // VMIN=0,VTIME=1 bounds a read to 100ms if SIGINT flushes the
                // byte BETWEEN poll readiness and read_some. No O_NONBLOCK
                // change leaks to the launcher's shared open-file description.
                raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 1;
                interactive_ = details::tcsetattr_noexcept(input_, TCSANOW, ::std::addressof(raw)) == 0;
                if(!interactive_) { details::active.store(3u, ::std::memory_order_release); return; }
                details::terminal_owner.store(1u, ::std::memory_order_relaxed);
                ::termios output_terminal{};
                interactive_display_ = interactive_ && details::tcgetattr_noexcept(::fast_io::out().native_handle(),
                    ::std::addressof(output_terminal)) == 0;
            }
            else if(terminal_required) { details::active.store(3u, ::std::memory_order_release); return; }
            // A rejected SA_RESETHAND delivery can retire this installation
            // while setup runs. CAS may publish ready ONLY from our installing
            // state; no unconditional store can resurrect a default disposition.
            expected = 2u;
            if(!details::active.compare_exchange_strong(expected, 1u, ::std::memory_order_acq_rel)) { return; }
#elif defined(_WIN32) && !defined(__CYGWIN__)
            auto const borrowed{::fast_io::win32::GetStdHandle(static_cast<::std::uint_least32_t>(STD_INPUT_HANDLE))};
            if(borrowed == nullptr || borrowed == INVALID_HANDLE_VALUE)
            { details::active.store(0u, ::std::memory_order_release); return; }
            ::HANDLE duplicate{};
            if(::uwvm2::utils::control::win32_abi::uwvm_DuplicateHandle(
                ::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(), borrowed,
                ::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(), ::std::addressof(duplicate),
                0u, FALSE, DUPLICATE_SAME_ACCESS) == 0)
            { details::active.store(0u, ::std::memory_order_release); return; }
            input_owner_.reset(duplicate); input_ = static_cast<::HANDLE>(input_owner_.native_handle());
            ::DWORD input_flags{};
            if(::uwvm2::utils::control::win32_abi::uwvm_GetHandleInformation( input_, ::std::addressof(input_flags)) == 0 ||
               (input_flags & HANDLE_FLAG_INHERIT) != 0u)
            { details::active.store(0u, ::std::memory_order_release); return; }
            input_type_ = ::fast_io::win32::GetFileType(input_);
            if(input_type_ == FILE_TYPE_CHAR)
            {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                try
                {
                    // Microsoft documents no public SDK declaration/import
                    // library for this export. FastIO owns the system32 DLL
                    // reference; the typed noexcept pointer borrows it only
                    // through this session. Missing export fails closed.
                    console_module_ = ::fast_io::native_dll_file{u"kernel32.dll", ::fast_io::dll_mode::win32_load_library_search_system32};
                    read_console_nowait_ = reinterpret_cast<details::read_console_nowait_fn>(
                        ::fast_io::dll_load_symbol(console_module_, "ReadConsoleInputExW"));
                }
                catch(...) { details::active.store(0u, ::std::memory_order_release); return; }
#else
                details::active.store(0u, ::std::memory_order_release); return;
#endif
                if(read_console_nowait_ == nullptr) { details::active.store(0u, ::std::memory_order_release); return; }
            }
            if(input_ == nullptr || input_ == INVALID_HANDLE_VALUE ||
               (input_type_ != FILE_TYPE_PIPE && input_type_ != FILE_TYPE_CHAR) ||
               details::set_console_handler_noexcept(details::interrupt_handler, TRUE) == 0)
            { details::active.store(0u, ::std::memory_order_release); return; }
            installed_ = true; details::active.store(1u, ::std::memory_order_release);
            if(::fast_io::win32::GetConsoleMode(input_, ::std::addressof(terminal_mode_)) != 0)
            {
                auto const mode{(terminal_mode_ | ENABLE_PROCESSED_INPUT) & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)};
                interactive_ = details::set_console_mode_noexcept(input_, mode) != 0;
            }
            if(input_type_ == FILE_TYPE_CHAR && !interactive_)
            { details::set_console_handler_noexcept(details::interrupt_handler, FALSE); installed_ = false; details::active.store(3u, ::std::memory_order_release); }
            if(interactive_)
            {
                output_ = ::fast_io::win32::GetStdHandle(static_cast<::std::uint_least32_t>(STD_OUTPUT_HANDLE));
                if(::fast_io::win32::GetConsoleMode(output_, ::std::addressof(output_mode_)) != 0)
                {
                    // UTF-8 output is emitted by fast_io; enable only the native
                    // terminal's VT cursor controls, then restore on normal exit.
                    auto const mode{output_mode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING};
                    output_mode_changed_ = details::set_console_mode_noexcept(output_, mode) != 0;
                    interactive_display_ = output_mode_changed_;
                }
            }
#else
            details::active.store(0u, ::std::memory_order_release);
#endif
        }
        native_session(native_session const&) = delete;
        native_session& operator=(native_session const&) = delete;
        ~native_session()
        {
            if(!installed_) { return; }
            details::active.store(3u, ::std::memory_order_release); // permanent retired state; callbacks own no session
#if defined(__unix__) || defined(__APPLE__)
            if(interactive_) { static_cast<void>(details::tcsetattr_noexcept(input_, TCSANOW, ::std::addressof(terminal_))); }
            struct ::sigaction current{};
            if(details::sigaction_noexcept(SIGINT, nullptr, ::std::addressof(current)) == 0 &&
               (current.sa_flags & SA_SIGINFO) != 0 && current.sa_sigaction == details::interrupt_handler)
            { static_cast<void>(details::sigaction_noexcept(SIGINT, ::std::addressof(previous_), nullptr)); }
#elif defined(_WIN32) && !defined(__CYGWIN__)
            if(interactive_) { static_cast<void>(details::set_console_mode_noexcept(input_, terminal_mode_)); }
            if(output_mode_changed_) { static_cast<void>(details::set_console_mode_noexcept(output_, output_mode_)); }
            static_cast<void>(details::set_console_handler_noexcept(details::interrupt_handler, FALSE));
#endif
            details::pending.store(0u, ::std::memory_order_relaxed);
        }
        [[nodiscard]] bool ready() const noexcept
        { return installed_ && details::active.load(::std::memory_order_acquire) == 1u; }
        [[nodiscard]] bool interactive() const noexcept { return interactive_; }
        [[nodiscard]] bool interactive_display() const noexcept { return interactive_display_; }
        [[nodiscard]] bool interrupt_pending() const noexcept
        { return ready() && details::pending.load(::std::memory_order_relaxed) != 0u; }
        [[nodiscard]] bool consume_interrupt() noexcept
        {
            auto const result{ready() && details::pending.exchange(0u, ::std::memory_order_relaxed) != 0u};
#if defined(__unix__) || defined(__APPLE__)
            // Normal manager context, not the handler: cancel already queued
            // terminal input as well as the editor's copied partial command.
            if(result && interactive_) { static_cast<void>(details::tcflush_noexcept(input_, TCIFLUSH)); }
#elif defined(_WIN32) && !defined(__CYGWIN__)
            if(result) { queued_size_ = queued_at_ = repeat_count_ = 0u; high_surrogate_ = 0u;
                if(interactive_) { static_cast<void>(details::flush_console_input_noexcept(input_)); } }
#endif
            return result;
        }
        [[nodiscard]] int read_byte() noexcept
        {
            if(!ready()) { return -2; }
            for(;;)
            {
                if(!ready()) { return -2; } // forwarded one-shot disposition may retire during poll
                if(consume_interrupt()) { return interrupted; }
#if defined(__unix__) || defined(__APPLE__)
                ::pollfd descriptor{input_, POLLIN, 0};
                auto const result{details::poll_noexcept(::std::addressof(descriptor), 1u, 100)};
                if(result < 0) { if(errno == EINTR) { continue; } return -2; }
                if(result == 0) { continue; }
                if((descriptor.revents & (POLLERR | POLLNVAL)) != 0) { return -2; }
                if((descriptor.revents & (POLLIN | POLLHUP)) == 0) { continue; }
# if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
                char byte{};
                // [one owned byte] end. FastIO borrows exactly one byte here;
                // no pointer advance or handler-owned FD occurs. For a Linux
                // FIFO another reader may steal readiness, but this independent
                // O_NONBLOCK description returns EAGAIN instead of blocking.
                auto const transferred{::fast_io::posix_read_nothrow(::fast_io::posix_io_observer{input_}, ::std::addressof(byte), 1u)};
                if(!ready()) { return -2; }
                if(consume_interrupt()) { return interrupted; }
                if(!transferred)
                {
                    if(transferred.error == EINTR || transferred.error == EAGAIN || transferred.error == EWOULDBLOCK) { continue; }
                    return -2;
                }
                if(transferred.transferred == 0u && interactive_ && (descriptor.revents & POLLHUP) == 0) { continue; }
                return transferred.transferred == 0u ? -1 : static_cast<unsigned char>(byte);
# endif
#elif defined(_WIN32) && !defined(__CYGWIN__)
                if(queued_at_ < queued_size_) { return static_cast<unsigned char>(queued_[queued_at_++]); }
                if(repeat_count_ != 0u)
                {
                    --repeat_count_;
                    if(queued_size_ != 0u) { queued_at_ = 1u; return static_cast<unsigned char>(queued_[0]); }
                    return repeat_key_;
                }
                if(interactive_)
                {
                    auto const wait{::fast_io::win32::WaitForSingleObject(input_, 100u)};
                    if(wait == WAIT_TIMEOUT) { continue; }
                    if(wait != WAIT_OBJECT_0) { return -2; }
                    ::INPUT_RECORD record{}; ::DWORD count{};
                    // [one complete owned INPUT_RECORD] end; count<=1.
                    // Even if another consumer takes the signaled record after
                    // WaitForSingleObject, NOWAIT returns with count==0. Never
                    // fall back to blocking ReadConsoleInputW on that race.
                    if(read_console_nowait_(input_, ::std::addressof(record), 1u,
                        ::std::addressof(count), details::console_read_nowait) == 0 || count > 1u) { return -2; }
                    if(consume_interrupt()) { return interrupted; }
                    if(count == 0u) { continue; }
                    if(record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) { continue; }
                    queued_size_ = queued_at_ = repeat_count_ = 0u;
                    auto const ch{static_cast<char16_t>(record.Event.KeyEvent.uChar.UnicodeChar)};
                    auto const controls{record.Event.KeyEvent.dwControlKeyState};
                    unsigned meta_byte{};
                    if((controls & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0u &&
                       (controls & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) == 0u)
                    {
                        // The actual VK prevents Alt+NumPad Unicode or unrelated
                        // modified keys from manufacturing an editor command.
                        // Some consoles report an Alt-letter with UnicodeChar=0;
                        // an exact letter VK still identifies that key.
                        // A zero UnicodeChar has no translated lower-case
                        // witness. Shift or CapsLock may change the letter, so
                        // neither may authorize the virtual-key fallback.
                        auto const zero_lowercase_witness{ch == 0u &&
                            (controls & (SHIFT_PRESSED | CAPSLOCK_ON)) == 0u};
                        switch(record.Event.KeyEvent.wVirtualKeyCode)
                        {
                        case 'F': if(ch == u'f' || zero_lowercase_witness) { meta_byte = 'f'; } break;
                        case 'B': if(ch == u'b' || zero_lowercase_witness) { meta_byte = 'b'; } break;
                        case 'D': if(ch == u'd' || zero_lowercase_witness) { meta_byte = 'd'; } break;
                        case 'Y': if(ch == u'y' || zero_lowercase_witness) { meta_byte = 'y'; } break;
                        case VK_BACK: if(ch == u'\b' || ch == 0u || ch == 127u) { meta_byte = 127u; } break;
                        default: break;
                        }
                    }
                    if(meta_byte != 0u)
                    {
                        // One translated KEY_EVENT record yields one ESC+meta
                        // editor action. Packed repeat counts are intentionally
                        // collapsed; separate records remain separate actions.
                        // Ctrl+Alt/AltGr stays in the ordinary Unicode path.
                        high_surrogate_ = 0u;
                        queued_[0] = static_cast<char8_t>(meta_byte);
                        queued_size_ = 1u; queued_at_ = 0u; repeat_count_ = 0u;
                        return 27;
                    }
                    if(ch == 0u)
                    {
                        switch(record.Event.KeyEvent.wVirtualKeyCode)
                        { case VK_UP: repeat_key_ = console_editing::key_up; break; case VK_DOWN: repeat_key_ = console_editing::key_down; break;
                          case VK_HOME: repeat_key_ = console_editing::key_home; break; case VK_END: repeat_key_ = console_editing::key_end; break;
                          case VK_DELETE: repeat_key_ = console_editing::key_delete; break; case VK_LEFT: repeat_key_ = 2; break;
                          case VK_RIGHT: repeat_key_ = 6; break; default: continue; }
                        repeat_count_ = record.Event.KeyEvent.wRepeatCount > 0u ? record.Event.KeyEvent.wRepeatCount - 1u : 0u;
                        return repeat_key_;
                    }
                    char16_t input[3]{}; input[0] = ch;
                    if(ch >= 0xd800u && ch <= 0xdbffu) { high_surrogate_ = ch; continue; }
                    if(ch >= 0xdc00u && ch <= 0xdfffu && high_surrogate_ != 0u)
                    { input[0] = high_surrogate_; input[1] = ch; }
                    high_surrogate_ = 0u;
# if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                    try
                    {
                        // [owned UTF16 input 1..2 units][terminator] end
                        // [safe] code_cvt receives the explicit local extent.
                        // One scalar/replacement encodes at most four UTF-8 bytes.
                        ::fast_io::basic_obuffer_view<char8_t> encoded{queued_, queued_ + sizeof(queued_)};
                        ::fast_io::io::print(encoded, ::fast_io::mnp::code_cvt(
                            ::fast_io::u16string_view{input, input[1] == 0u ? 1u : 2u}));
                        queued_size_ = static_cast<unsigned>(encoded.curr_ptr - queued_); queued_at_ = 0u;
                        if(queued_size_ == 0u || queued_size_ > sizeof(queued_)) { return -2; }
                        repeat_count_ = record.Event.KeyEvent.wRepeatCount > 0u ? record.Event.KeyEvent.wRepeatCount - 1u : 0u;
                        return static_cast<unsigned char>(queued_[queued_at_++]);
                    }
                    catch(::fast_io::error const&) { return -2; }
# else
                    return -2;
# endif
                }
                ::DWORD available{};
                if(::uwvm2::utils::control::win32_abi::uwvm_PeekNamedPipe(input_, nullptr, 0u, nullptr, ::std::addressof(available), nullptr) == 0)
                { return ::fast_io::win32::GetLastError() == ERROR_BROKEN_PIPE ? -1 : -2; }
                if(available == 0u) { ::fast_io::win32::Sleep(100u); continue; }
                auto const next{read_pipe_byte()};
                if(next == -4) { continue; } // stolen Peek readiness, worker canceled and joined
                return next;
#endif
#if !(defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__))
# if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                try
                {
                    char byte{};
                    // [one owned byte] end; read_some receives exactly its extent.
#if defined(__unix__) || defined(__APPLE__)
                    auto const next{::fast_io::operations::read_some(::fast_io::posix_io_observer{input_}, ::std::addressof(byte), ::std::addressof(byte) + 1u)};
#else
                    auto const next{::fast_io::operations::read_some(::fast_io::win32_io_observer{input_}, ::std::addressof(byte), ::std::addressof(byte) + 1u)};
#endif
                    if(!ready()) { return -2; }
                    if(consume_interrupt()) { return interrupted; } // interrupted partial input is never executed
#if defined(__unix__) || defined(__APPLE__)
                    if(next == ::std::addressof(byte) && interactive_ && (descriptor.revents & POLLHUP) == 0) { continue; }
#endif
                    return next == ::std::addressof(byte) ? -1 : static_cast<unsigned char>(byte);
                }
                catch(::fast_io::error const&) { if(consume_interrupt()) { return interrupted; } return -2; }
# else
                return -2; // no qualified noEH error-returning input provider
# endif
#endif
            }
        }
    };
}
