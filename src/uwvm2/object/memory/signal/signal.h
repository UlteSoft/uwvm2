/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
// platfrom
# include <signal.h>
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

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::signal
{
    struct protected_memory_segment_t
    {
        ::std::byte const* begin{};
        ::std::byte const* end{};
        ::std::atomic_size_t const* length_p{};
        ::std::size_t memory_idx{};
        ::std::uint_least64_t memory_static_offset{};
    };

    namespace detail
    {
        inline ::std::vector<protected_memory_segment_t> segments{};  // [global]

        struct signal_handlers_t
        {
#if defined(_WIN32) || defined(__CYGWIN__)
            void* vectored_handler_handle{};
#else
            ::sigaction previous_sigsegv{};
            ::sigaction previous_sigbus{};
            bool has_previous_sigsegv{};
            bool has_previous_sigbus{};
#endif
        };

        inline signal_handlers_t signal_handlers{};  // [global]

        inline constexpr ::std::uint_least64_t get_memory_length(protected_memory_segment_t const& seg) noexcept

        {
            if(seg.length_p != nullptr) [[likely]] { return static_cast<::std::uint_least64_t>(seg.length_p->load(::std::memory_order_acquire)); }

            return static_cast<::std::uint_least64_t>(seg.end - seg.begin);
        }

        inline constexpr ::uwvm2::object::memory::error::memory_error_t make_memory_error(protected_memory_segment_t const& seg,
                                                                                          ::std::byte const* fault_addr) noexcept
        {
            auto const offset{static_cast<::std::uint_least64_t>(fault_addr - seg.begin)};
            auto const memory_length{get_memory_length(seg)};

            return {
                .memory_idx = seg.memory_idx,
                .memory_offset = {.offset = offset, .offset_65_bit = false},
                .memory_static_offset = seg.memory_static_offset,
                .memory_length = memory_length,
                .memory_type_size = 1uz
            };
        }

        inline constexpr bool handle_fault_address(::std::byte const* fault_addr) noexcept
        {
            if(fault_addr == nullptr) [[unlikely]] { return false; }

            for(auto const& seg: segments)
            {
                if(seg.begin <= fault_addr && fault_addr < seg.end)
                {
                    auto const memerr{make_memory_error(seg, fault_addr)};
                    ::uwvm2::object::memory::error::output_memory_error_and_terminate(memerr);
                    return true;
                }
            }

            return false;
        }

#if defined(_WIN32) || defined(__CYGWIN__)
        inline constexpr ::std::int_least32_t UWVM_WINSTDCALL vectored_exception_handler(::fast_io::win32::exception_pointers* exception_pointers) noexcept
        {
            if(exception_pointers == nullptr || exception_pointers->ExceptionRecord == nullptr) [[unlikely]] { return 0 /*EXCEPTION_CONTINUE_SEARCH*/; }

            auto const code{exception_pointers->ExceptionRecord->ExceptionCode};
            if(code ==
# if defined(_WIN32_WINDOWS)
               998 /*EXCEPTION_ACCESS_VIOLATION*/
# else
               0xC0000005u /*STATUS_ACCESS_VIOLATION*/
# endif
            )
            {
                if(exception_pointers->ExceptionRecord->NumberParameters >= 2u)
                {
                    auto const fault_addr{reinterpret_cast<::std::byte const*>(exception_pointers->ExceptionRecord->ExceptionInformation[1u])};

                    if(handle_fault_address(fault_addr)) { return -1 /*EXCEPTION_CONTINUE_EXECUTION*/; }
                }
            }

            return 0 /*EXCEPTION_CONTINUE_SEARCH*/;
        }

        inline constexpr void install_signal_handler() noexcept
        {
            static ::std::atomic_bool signal_installed{};  // [global]

            // call once
            bool expected{false};
            if(!signal_installed.compare_exchange_strong(expected, true, ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return; }

            signal_handlers.vectored_handler_handle = ::fast_io::win32::AddVectoredExceptionHandler(1u, vectored_exception_handler);
            if(signal_handlers.vectored_handler_handle == nullptr) [[unlikely]]
            {
# ifdef UWVM
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Failed to install signal handler.\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
                ::fast_io::io::perr(::fast_io::u8err(), u8"uwvm: [fatal] Failed to install signal handler.\n\n");
# endif
                ::fast_io::fast_terminate();
            }
        }

        inline constexpr void install_protected_memory_segment(protected_memory_segment_t const& seg) noexcept
        {
            segments.push_back(seg);
        }

        inline constexpr void install_protected_memory_segment(protected_memory_segment_t && seg) noexcept
        {
            segments.push_back(::std::move(seg));
        }
#else
        inline constexpr void dispatch_previous_handler(int signal, ::siginfo_t* siginfo, void* context, ::sigaction const& previous) noexcept
        {
            if(previous.sa_flags & SA_SIGINFO)
            {
                if(previous.sa_sigaction == SIG_DFL)
                {
                    ::signal(signal, SIG_DFL);
                    ::raise(signal);
                    return;
                }

                if(previous.sa_sigaction == SIG_IGN) { return; }

                previous.sa_sigaction(signal, siginfo, context);
                return;
            }

            if(previous.sa_handler == SIG_DFL)
            {
                ::signal(signal, SIG_DFL);
                ::raise(signal);
                return;
            }

            if(previous.sa_handler == SIG_IGN) { return; }

            previous.sa_handler(signal);
        }

        inline void posix_signal_handler(int signal, ::siginfo_t* siginfo, void* context) noexcept
        {
            auto const fault_addr{siginfo == nullptr ? nullptr : reinterpret_cast<::std::byte const*>(siginfo->si_addr)};

            if(handle_fault_address(fault_addr)) { return; }

            if(signal == SIGSEGV && has_previous_sigsegv)
            {
                dispatch_previous_handler(signal, siginfo, context, previous_sigsegv);
                return;
            }

# ifdef SIGBUS
            if(signal == SIGBUS && has_previous_sigbus)
            {
                dispatch_previous_handler(signal, siginfo, context, previous_sigbus);
                return;
            }
# endif

            ::signal(signal, SIG_DFL);
            ::raise(signal);
        }

        inline void install_signal_handler() noexcept
        {
            bool expected{false};
            if(!signal_installed.compare_exchange_strong(expected, true, ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return; }

            ::sigaction act{};
            act.sa_sigaction = posix_signal_handler;
            ::sigemptyset(::std::addressof(act.sa_mask));
            act.sa_flags = SA_SIGINFO;

            if(::sigaction(SIGSEGV, ::std::addressof(act), ::std::addressof(previous_sigsegv)) != 0) [[unlikely]] { ::fast_io::fast_terminate(); }

            has_previous_sigsegv = true;

# ifdef SIGBUS
            if(::sigaction(SIGBUS, ::std::addressof(act), ::std::addressof(previous_sigbus)) != 0) [[unlikely]] { ::fast_io::fast_terminate(); }

            has_previous_sigbus = true;
# endif
        }
#endif
    }  // namespace detail

    inline void register_protected_segment(::std::byte const* begin,
                                           ::std::byte const* end,
                                           ::std::atomic_size_t const* length_p = nullptr,
                                           ::std::size_t memory_idx = 0uz,
                                           ::std::uint_least64_t memory_static_offset = 0u) noexcept
    {
        if(begin == nullptr || end == nullptr || begin >= end) [[unlikely]] { ::fast_io::fast_terminate(); }

        detail::install_signal_handler();

        auto& segments{detail::tracked_segments()};

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            segments.push_back({begin, end, length_p, memory_idx, memory_static_offset});
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(...)
        {
            ::fast_io::fast_terminate();
        }
#endif
    }

    inline void unregister_protected_segment(::std::byte const* begin, ::std::byte const* end) noexcept
    {
        if(begin == nullptr || end == nullptr) [[unlikely]] { return; }

        auto& segments{detail::tracked_segments()};
        for(auto it{segments.begin()}; it != segments.end(); ++it)
        {
            if(it->begin == begin && it->end == end)
            {
                segments.erase(it);
                return;
            }
        }
    }

    inline void clear_protected_segments() noexcept { detail::tracked_segments().clear(); }

}  // namespace uwvm2::object::memory::signal

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif
