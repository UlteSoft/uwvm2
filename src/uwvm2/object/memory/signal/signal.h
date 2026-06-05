/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <limits>
# include <concepts>
# include <bit>
# include <vector>
# include <atomic>
# include <memory>
# include <utility>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
// platfrom
# include <signal.h>
# if !(defined(_WIN32) || defined(__CYGWIN__)) && defined(__has_include)
#  if __has_include(<ucontext.h>)
#   include <ucontext.h>
#   define UWVM2_OBJECT_MEMORY_SIGNAL_HAS_UCONTEXT
#  endif
# endif
// import
# include <fast_io.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/object/memory/error/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_SUPPORT_MMAP)

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::signal
{
# if !(defined(_WIN32) || defined(__CYGWIN__))
    namespace posix
    {
        extern int raise(int) noexcept
#  if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("raise")
#  else
            __asm__("_raise")
#  endif
                ;

        extern int sigaction(int, struct ::sigaction const*, struct ::sigaction*) noexcept
#  if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("sigaction")
#  else
            __asm__("_sigaction")
#  endif
                ;

        extern void (*signal(int, void (*)(int)))(int) noexcept
#  if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("signal")
#  else
            __asm__("_signal")
#  endif
                ;
    }  // namespace posix
# endif

    struct protected_memory_segment_t
    {
        ::std::byte const* begin{};
        ::std::byte const* end{};
        ::std::atomic_size_t const* length_p{};
        ::std::size_t memory_idx{};
    };

    using mmap_memory_out_of_bounds_func_t = void (*)(::uwvm2::object::memory::error::mmap_memory_error_t const&) noexcept;

    namespace detail
    {
        inline ::uwvm2::utils::container::vector<protected_memory_segment_t> segments{};  // [global]
        inline mmap_memory_out_of_bounds_func_t mmap_memory_out_of_bounds_func{};         // [global]

        struct signal_handlers_t
        {
# if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0501)
            ::fast_io::win32::pvectored_exception_handler previous_unhandled_exception_filter{};
#  else
            void* vectored_handler_handle{};
#  endif
# else
            struct ::sigaction previous_sigsegv{};
            struct ::sigaction previous_sigbus{};
            bool has_previous_sigsegv{};
            bool has_previous_sigbus{};
# endif
        };

        inline signal_handlers_t signal_handlers{};  // [global]

        inline constexpr ::std::size_t get_memory_length(protected_memory_segment_t const& seg) noexcept
        {
            if(seg.length_p != nullptr) [[likely]] { return static_cast<::std::size_t>(seg.length_p->load(::std::memory_order_acquire)); }

            return static_cast<::std::size_t>(seg.end - seg.begin);
        }

        inline constexpr ::uwvm2::object::memory::error::mmap_memory_error_t
            make_mmap_memory_error(protected_memory_segment_t const& seg,
                                   ::std::byte const* fault_addr,
                                   ::std::uintptr_t instruction_address) noexcept
        {
            auto const offset{static_cast<::std::size_t>(fault_addr - seg.begin)};
            auto const memory_length{get_memory_length(seg)};

            return {.memory_idx = seg.memory_idx,
                    .memory_offset = static_cast<::std::uint_least64_t>(offset),
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .instruction_address = instruction_address};
        }

        inline bool handle_fault_address(::std::byte const* fault_addr, ::std::uintptr_t instruction_address) noexcept
        {
            if(fault_addr == nullptr) [[unlikely]] { return false; }

            for(auto const& seg: segments)
            {
                if(seg.begin <= fault_addr && fault_addr < seg.end)
                {
                    auto const mmapmemerr{make_mmap_memory_error(seg, fault_addr, instruction_address)};
                    if(auto const handler{mmap_memory_out_of_bounds_func}; handler != nullptr) [[likely]]
                    {
                        handler(mmapmemerr);
                        ::fast_io::fast_terminate();
                        ::std::unreachable();
                    }

                    ::uwvm2::object::memory::error::output_mmap_memory_error_and_terminate(mmapmemerr);
                    ::std::unreachable();
                }
            }

            return false;
        }

# if defined(_WIN32) || defined(__CYGWIN__)
        inline ::std::int_least32_t UWVM_WINSTDCALL vectored_exception_handler(::fast_io::win32::exception_pointers* exception_pointers) noexcept
        {
            if(exception_pointers == nullptr || exception_pointers->ExceptionRecord == nullptr) [[unlikely]] { return 0 /*EXCEPTION_CONTINUE_SEARCH*/; }

            auto const code{exception_pointers->ExceptionRecord->ExceptionCode};
            if(code ==
#  if defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0501)
               998 /*EXCEPTION_ACCESS_VIOLATION*/
#  else
               0xC0000005u /*STATUS_ACCESS_VIOLATION*/
#  endif
            )
            {
                if(exception_pointers->ExceptionRecord->NumberParameters >= 2u)
                {
                    auto const fault_addr{reinterpret_cast<::std::byte const*>(exception_pointers->ExceptionRecord->ExceptionInformation[1u])};
                    auto const instruction_address{reinterpret_cast<::std::uintptr_t>(exception_pointers->ExceptionRecord->ExceptionAddress)};

                    if(handle_fault_address(fault_addr, instruction_address)) { return -1 /*EXCEPTION_CONTINUE_EXECUTION*/; }
                }
            }

            return 0 /*EXCEPTION_CONTINUE_SEARCH*/;
        }

#  if defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0501)
        inline ::std::int_least32_t UWVM_WINSTDCALL unhandled_exception_filter(::fast_io::win32::exception_pointers* exception_pointers) noexcept
        {
            auto const ret{vectored_exception_handler(exception_pointers)};
            if(ret != 0) { return ret; }

            auto const prev{signal_handlers.previous_unhandled_exception_filter};
            if(prev != nullptr && prev != unhandled_exception_filter) { return prev(exception_pointers); }

            return 0 /*EXCEPTION_CONTINUE_SEARCH*/;
        }
#  endif

        inline void install_signal_handler() noexcept
        {
            static ::std::atomic_bool signal_installed{};  // [global]

            // call once
            bool expected{false};
            if(!signal_installed.compare_exchange_strong(expected, true, ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return; }

            auto& handlers{signal_handlers};

            // NOTE: In C++17 and later, for `E1 = E2`, `E2` is sequenced before `E1`.
            // Keep handler-state evaluation separated from installing the handler to avoid re-entrancy
            // pitfalls if `signal_handlers` ever becomes lazily initialized.
#  if defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0501)
            handlers.previous_unhandled_exception_filter = ::fast_io::win32::SetUnhandledExceptionFilter(unhandled_exception_filter);
#  else
            handlers.vectored_handler_handle = ::fast_io::win32::AddVectoredExceptionHandler(1u, vectored_exception_handler);
            if(handlers.vectored_handler_handle == nullptr) [[unlikely]]
            {
#   ifdef UWVM
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Failed to install signal handler.\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
#   else
                ::fast_io::io::perr(::fast_io::u8err(), u8"uwvm: [fatal] Failed to install signal handler.\n\n");
#   endif
                ::fast_io::fast_terminate();
            }
#  endif
        }

# else
        [[nodiscard]] inline ::std::uintptr_t get_signal_instruction_address(void* context) noexcept
        {
#  ifdef UWVM2_OBJECT_MEMORY_SIGNAL_HAS_UCONTEXT
            if(context == nullptr) [[unlikely]] { return 0u; }
            auto const uctx{static_cast<::ucontext_t const*>(context)};

#   if defined(__linux__) && defined(__x86_64__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.gregs[REG_RIP]);
#   elif defined(__linux__) && defined(__i386__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.gregs[REG_EIP]);
#   elif defined(__linux__) && defined(__aarch64__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.pc);
#   elif defined(__linux__) && defined(__arm__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.arm_pc);
#   elif defined(__linux__) && defined(__riscv) && __riscv_xlen == 64
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[REG_PC]);
#   elif defined(__APPLE__) && defined(__x86_64__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext->__ss.__rip);
#   elif defined(__APPLE__) && defined(__aarch64__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext->__ss.__pc);
#   else
            static_cast<void>(uctx);
            return 0u;
#   endif
#  else
            static_cast<void>(context);
            return 0u;
#  endif
        }

        inline constexpr void dispatch_previous_handler(int signal, ::siginfo_t* siginfo, void* context, struct ::sigaction const& previous) noexcept
        {
            if(previous.sa_flags & SA_SIGINFO)
            {
                if(reinterpret_cast<void*>(previous.sa_sigaction) == reinterpret_cast<void*>(SIG_DFL))
                {
                    posix::signal(signal, SIG_DFL);
                    posix::raise(signal);
                    return;
                }

                if(reinterpret_cast<void*>(previous.sa_sigaction) == reinterpret_cast<void*>(SIG_IGN)) { return; }

                previous.sa_sigaction(signal, siginfo, context);
                return;
            }

            if(reinterpret_cast<void*>(previous.sa_handler) == reinterpret_cast<void*>(SIG_DFL))
            {
                posix::signal(signal, SIG_DFL);
                posix::raise(signal);
                return;
            }

            if(reinterpret_cast<void*>(previous.sa_handler) == reinterpret_cast<void*>(SIG_IGN)) { return; }

            previous.sa_handler(signal);
        }

        inline void posix_signal_handler(int signal, ::siginfo_t* siginfo, void* context) noexcept
        {
            auto const fault_addr{siginfo == nullptr ? nullptr : reinterpret_cast<::std::byte const*>(siginfo->si_addr)};
            auto const instruction_address{get_signal_instruction_address(context)};

            if(handle_fault_address(fault_addr, instruction_address)) { return; }

            if(signal == SIGSEGV && signal_handlers.has_previous_sigsegv)
            {
                dispatch_previous_handler(signal, siginfo, context, signal_handlers.previous_sigsegv);
                return;
            }

#  ifdef SIGBUS
            if(signal == SIGBUS && signal_handlers.has_previous_sigbus)
            {
                dispatch_previous_handler(signal, siginfo, context, signal_handlers.previous_sigbus);
                return;
            }
#  endif

            posix::signal(signal, SIG_DFL);
            posix::raise(signal);
        }

        inline void install_signal_handler() noexcept
        {
            static ::std::atomic_bool signal_installed{};  // [global]

            bool expected{false};
            if(!signal_installed.compare_exchange_strong(expected, true, ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return; }

            struct ::sigaction act{};
            act.sa_sigaction = posix_signal_handler;
            sigemptyset(::std::addressof(act.sa_mask));
            act.sa_flags = SA_SIGINFO;

            if(posix::sigaction(SIGSEGV, ::std::addressof(act), ::std::addressof(signal_handlers.previous_sigsegv)) != 0) [[unlikely]]
            {
#  ifdef UWVM
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Failed to install signal handler (SIGSEGV).\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
#  else
                ::fast_io::io::perr(::fast_io::u8err(), u8"uwvm: [fatal] Failed to install signal handler (SIGSEGV).\n\n");
#  endif
                ::fast_io::fast_terminate();
            }
            signal_handlers.has_previous_sigsegv = true;

#  ifdef SIGBUS
            if(posix::sigaction(SIGBUS, ::std::addressof(act), ::std::addressof(signal_handlers.previous_sigbus)) != 0) [[unlikely]]
            {
#   ifdef UWVM
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Failed to install signal handler (SIGBUS).\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
#   else
                ::fast_io::io::perr(::fast_io::u8err(), u8"uwvm: [fatal] Failed to install signal handler (SIGBUS).\n\n");
#   endif
                ::fast_io::fast_terminate();
            }
            signal_handlers.has_previous_sigbus = true;
#  endif
        }
# endif
    }  // namespace detail

    /// @note       Protected memory segments are expected to be registered during VM/module initialization,
    ///             before any guest code that may trigger memory access violations is executed. After
    ///             initialization, the set of segments is treated as structurally immutable (only the
    ///             dynamic length, when provided via @p length_p, may change), and no further calls to
    ///             register_protected_segment, unregister_protected_segment, or clear_protected_segments
    ///             are performed. This design ensures that the signal/exception handler can traverse the
    ///             global segments container without additional synchronization, as there are no
    ///             concurrent structural modifications.

    inline constexpr void set_mmap_memory_out_of_bounds_handler(mmap_memory_out_of_bounds_func_t func) noexcept
    { detail::mmap_memory_out_of_bounds_func = func; }

    inline constexpr void register_protected_segment(::std::byte const* begin,
                                                     ::std::byte const* end,
                                                     ::std::atomic_size_t const* length_p = nullptr,
                                                     ::std::size_t memory_idx = 0uz) noexcept
    {
        if(begin == nullptr || end == nullptr || begin >= end) [[unlikely]]
        {
# ifdef UWVM
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid protected memory segment.\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(), u8"uwvm: [fatal] Invalid protected memory segment.\n\n");
# endif
            ::fast_io::fast_terminate();
        }

        detail::install_signal_handler();
        detail::segments.emplace_back(begin, end, length_p, memory_idx);
    }

    inline constexpr void unregister_protected_segment(::std::byte const* begin, ::std::byte const* end) noexcept
    {
        if(begin == nullptr || end == nullptr) [[unlikely]] { return; }

        auto& segments{detail::segments};
        for(auto it{segments.begin()}; it != segments.end(); ++it)
        {
            if(it->begin == begin && it->end == end)
            {
                segments.erase(it);
                return;
            }
        }
    }

    inline constexpr void clear_protected_segments() noexcept { detail::segments.clear(); }

}  // namespace uwvm2::object::memory::signal

#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif
