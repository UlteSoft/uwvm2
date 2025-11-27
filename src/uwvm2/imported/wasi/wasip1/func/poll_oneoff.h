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
# include <climits>
# include <cstring>
# include <limits>
# include <concepts>
# include <bit>
# include <memory>
# include <type_traits>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
// platform
# if !defined(_WIN32)
#  include <errno.h>
#  if __has_include(<poll.h>)
#   include <poll.h>
#  endif
#  if defined(__linux__)
#   if __has_include(<sys/epoll.h>)
#    include <sys/epoll.h>
#   endif
#   if __has_include(<sys/timerfd.h>)
#    include <sys/timerfd.h>
#   endif
#  endif
#  if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || defined(BSD) || defined(_SYSTYPE_BSD) ||         \
      (defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
#   if __has_include(<sys/event.h>)
#    include <sys/event.h>
#   endif
#   if __has_include(<sys/time.h>)
#    include <sys/time.h>
#   endif
#  endif
# endif
// import
# include <fast_io.h>
# include <fast_io_device.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/object/memory/linear/impl.h>
# include <uwvm2/imported/wasi/wasip1/abi/impl.h>
# include <uwvm2/imported/wasi/wasip1/fd_manager/impl.h>
# include <uwvm2/imported/wasi/wasip1/memory/impl.h>
# include <uwvm2/imported/wasi/wasip1/environment/impl.h>
# include "base.h"
# include "posix.h"
#endif

#ifndef UWVM_CPP_EXCEPTIONS
# warning "Without enabling C++ exceptions, using this WASI function may cause termination."
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{
    // Local representations of WASI poll structs, layout-compatible with wasi-libc's
    // __wasi_event_t / __wasi_subscription_t on wasm32. These are intentionally kept
    // internal to this translation unit and are not part of the public ABI namespace.

    struct alignas(8uz) wasi_event_fd_readwrite_t
    {
        ::uwvm2::imported::wasi::wasip1::abi::filesize_t nbytes;
        ::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t flags;
    };

    inline constexpr ::std::size_t size_of_wasi_event_fd_readwrite_t{16uz};

    inline consteval bool is_default_wasi_event_fd_readwrite_data_layout() noexcept
    {
        return __builtin_offsetof(wasi_event_fd_readwrite_t, nbytes) == 0uz && __builtin_offsetof(wasi_event_fd_readwrite_t, flags) == 8uz &&
               sizeof(wasi_event_fd_readwrite_t) == size_of_wasi_event_fd_readwrite_t && alignof(wasi_event_fd_readwrite_t) == 8uz &&
               ::std::endian::native == ::std::endian::little;
    }

    struct alignas(8uz) wasi_event_t
    {
        ::uwvm2::imported::wasi::wasip1::abi::userdata_t userdata;
        ::uwvm2::imported::wasi::wasip1::abi::errno_t error;
        ::uwvm2::imported::wasi::wasip1::abi::eventtype_t type;

        union wasi_event_u
        {
            wasi_event_fd_readwrite_t fd_readwrite;
        } u;
    };

    inline constexpr ::std::size_t size_of_wasi_event_t{32uz};

    inline consteval bool is_default_wasi_event_data_layout() noexcept
    {
        return __builtin_offsetof(wasi_event_t, userdata) == 0uz && __builtin_offsetof(wasi_event_t, error) == 8uz &&
               __builtin_offsetof(wasi_event_t, type) == 10uz && __builtin_offsetof(wasi_event_t, u) == 16uz && sizeof(wasi_event_t) == size_of_wasi_event_t &&
               alignof(wasi_event_t) == 8uz && ::std::endian::native == ::std::endian::little && is_default_wasi_event_fd_readwrite_data_layout();
    }

    struct alignas(8uz) wasi_subscription_clock_t
    {
        ::uwvm2::imported::wasi::wasip1::abi::clockid_t id;
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t timeout;
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t precision;
        ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t flags;
    };

    inline constexpr ::std::size_t size_of_wasi_subscription_clock_t{32uz};

    inline consteval bool is_default_wasi_subscription_clock_data_layout() noexcept
    {
        return __builtin_offsetof(wasi_subscription_clock_t, id) == 0uz && __builtin_offsetof(wasi_subscription_clock_t, timeout) == 8uz &&
               __builtin_offsetof(wasi_subscription_clock_t, precision) == 16uz && __builtin_offsetof(wasi_subscription_clock_t, flags) == 24uz &&
               sizeof(wasi_subscription_clock_t) == size_of_wasi_subscription_clock_t && alignof(wasi_subscription_clock_t) == 8uz &&
               ::std::endian::native == ::std::endian::little;
    }

    struct alignas(4uz) wasi_subscription_fd_readwrite_t
    {
        ::uwvm2::imported::wasi::wasip1::abi::fd_t file_descriptor;
    };

    inline constexpr ::std::size_t size_of_wasi_subscription_fd_readwrite_t{4uz};

    inline consteval bool is_default_wasi_subscription_fd_readwrite_data_layout() noexcept
    {
        return __builtin_offsetof(wasi_subscription_fd_readwrite_t, file_descriptor) == 0uz &&
               sizeof(wasi_subscription_fd_readwrite_t) == size_of_wasi_subscription_fd_readwrite_t && alignof(wasi_subscription_fd_readwrite_t) == 4uz &&
               ::std::endian::native == ::std::endian::little;
    }

    struct alignas(8uz) wasi_subscription_u_t
    {
        ::uwvm2::imported::wasi::wasip1::abi::eventtype_t tag;

        union payload_u
        {
            wasi_subscription_clock_t clock;
            wasi_subscription_fd_readwrite_t fd_readwrite;
        } u;
    };

    inline constexpr ::std::size_t size_of_wasi_subscription_u_t{40uz};

    inline consteval bool is_default_wasi_subscription_u_data_layout() noexcept
    {
        return __builtin_offsetof(wasi_subscription_u_t, tag) == 0uz && __builtin_offsetof(wasi_subscription_u_t, u) == 8uz &&
               sizeof(wasi_subscription_u_t) == size_of_wasi_subscription_u_t && alignof(wasi_subscription_u_t) == 8uz &&
               ::std::endian::native == ::std::endian::little && is_default_wasi_subscription_clock_data_layout() &&
               is_default_wasi_subscription_fd_readwrite_data_layout();
    }

    struct alignas(8uz) wasi_subscription_t
    {
        ::uwvm2::imported::wasi::wasip1::abi::userdata_t userdata;
        wasi_subscription_u_t u;
    };

    inline constexpr ::std::size_t size_of_wasi_subscription_t{48uz};

    inline consteval bool is_default_wasi_subscription_data_layout() noexcept
    {
        return __builtin_offsetof(wasi_subscription_t, userdata) == 0uz && __builtin_offsetof(wasi_subscription_t, u) == 8uz &&
               sizeof(wasi_subscription_t) == size_of_wasi_subscription_t && alignof(wasi_subscription_t) == 8uz &&
               ::std::endian::native == ::std::endian::little && is_default_wasi_subscription_u_data_layout();
    }

    /// @brief     WasiPreview1.poll_oneoff
    /// @details   __wasi_errno_t __wasi_poll_oneoff(const __wasi_subscription_t *in,
    ///                                              __wasi_event_t *out,
    ///                                              __wasi_size_t nsubscriptions,
    ///                                              __wasi_size_t *nevents);
    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t poll_oneoff(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t in,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t out,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t nsubscriptions,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t nevents) noexcept
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(env.wasip1_memory == nullptr) [[unlikely]]
        {
            // Security issues inherent to virtual machines
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        auto& memory{*env.wasip1_memory};

        auto const trace_wasip1_call{env.trace_wasip1_call};

        if(trace_wasip1_call) [[unlikely]]
        {
#ifdef UWVM
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"wasip1: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"poll_oneoff",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(in),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(out),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                nsubscriptions,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(nevents),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                u8"\n");
#else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: poll_oneoff(",
                                ::fast_io::mnp::addrvw(in),
                                u8", ",
                                ::fast_io::mnp::addrvw(out),
                                u8", ",
                                nsubscriptions,
                                u8", ",
                                ::fast_io::mnp::addrvw(nevents),
                                u8") (wasi-trace)\n");
#endif
        }

        // Early exit: zero subscriptions -> zero events.
        if(nsubscriptions == 0u) { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval; }

        // Check memory bounds for the input and output arrays.
        // This protects against overflow when multiplying by the element size.
        constexpr ::std::size_t max_size_t{::std::numeric_limits<::std::size_t>::max()};

        constexpr ::std::size_t max_size_t_div_size_of_wasi_subscription_t{max_size_t / size_of_wasi_subscription_t};
        if constexpr(::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>::max() > max_size_t_div_size_of_wasi_subscription_t)
        {
            if(nsubscriptions > max_size_t_div_size_of_wasi_subscription_t) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow; }
        }

        auto const subs_bytes{static_cast<::std::size_t>(nsubscriptions) * size_of_wasi_subscription_t};

        constexpr ::std::size_t max_size_t_div_size_of_wasi_event_t{max_size_t / size_of_wasi_event_t};
        if constexpr(::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>::max() > max_size_t_div_size_of_wasi_event_t)
        {
            if(nsubscriptions > max_size_t_div_size_of_wasi_event_t) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow; }
        }

        auto const events_bytes{static_cast<::std::size_t>(nsubscriptions) * size_of_wasi_event_t};

        ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm32(memory, in, subs_bytes);
        ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm32(memory, out, events_bytes);

        ::uwvm2::utils::container::vector<wasi_subscription_t> subscriptions{};
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t curr_in_pos{in};

        subscriptions.reserve(nsubscriptions);

        {
            [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

            for(::uwvm2::imported::wasi::wasip1::abi::wasi_size_t i{}; i != nsubscriptions; ++i)
            {
                wasi_subscription_t tmp_subscription{};

                if constexpr(is_default_wasi_subscription_data_layout())
                {
                    ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm32_unchecked_unlocked(
                        memory,
                        curr_in_pos,
                        reinterpret_cast<::std::byte*>(::std::addressof(tmp_subscription)),
                        reinterpret_cast<::std::byte*>(::std::addressof(tmp_subscription)) + sizeof(tmp_subscription));

                    curr_in_pos += static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(size_of_wasi_subscription_t);
                }
                else
                {
                    using userdata_underlying_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::userdata_t>;

                    tmp_subscription.userdata = static_cast<::uwvm2::imported::wasi::wasip1::abi::userdata_t>(
                        ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked<userdata_underlying_t>(memory,
                                                                                                                                                  curr_in_pos +
                                                                                                                                                      0u));

                    using eventtype_underlying_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventtype_t>;

                    tmp_subscription.u.tag = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventtype_t>(
                        ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked<eventtype_underlying_t>(memory,
                                                                                                                                                   curr_in_pos +
                                                                                                                                                       8u));

                    if constexpr(is_default_wasi_subscription_u_data_layout())
                    {
                        ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm32_unchecked_unlocked(
                            memory,
                            curr_in_pos + 8u,
                            reinterpret_cast<::std::byte*>(::std::addressof(tmp_subscription.u)),
                            reinterpret_cast<::std::byte*>(::std::addressof(tmp_subscription.u)) + sizeof(tmp_subscription.u));
                    }
                    else
                    {
                        auto const curr_union_base{static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(curr_in_pos + 16u)};

                        switch(tmp_subscription.u.tag)
                        {
                            case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock:
                            {
                                using clockid_underlying_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::clockid_t>;
                                using timestamp_underlying_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;
                                using subclockflags_underlying_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::subclockflags_t>;

                                auto const id_val{
                                    ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked<clockid_underlying_t>(
                                        memory,
                                        curr_union_base + 0u)};

                                auto const timeout_val{
                                    ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked<timestamp_underlying_t>(
                                        memory,
                                        curr_union_base + 8u)};

                                auto const precision_val{
                                    ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked<timestamp_underlying_t>(
                                        memory,
                                        curr_union_base + 16u)};

                                auto const flags_val{::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked<
                                    subclockflags_underlying_t>(memory, curr_union_base + 24u)};

                                tmp_subscription.u.u.clock.id = static_cast<::uwvm2::imported::wasi::wasip1::abi::clockid_t>(id_val);
                                tmp_subscription.u.u.clock.timeout = static_cast<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(timeout_val);
                                tmp_subscription.u.u.clock.precision = static_cast<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(precision_val);
                                tmp_subscription.u.u.clock.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::subclockflags_t>(flags_val);

                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_read: [[fallthrough]];
                            case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write:
                            {
                                using fd_underlying_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::fd_t>;

                                auto const fd_val{
                                    ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked<fd_underlying_t>(
                                        memory,
                                        curr_union_base + 0u)};

                                tmp_subscription.u.u.fd_readwrite.file_descriptor = static_cast<::uwvm2::imported::wasi::wasip1::abi::fd_t>(fd_val);

                                break;
                            }
                            [[unlikely]] default:
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                            }
                        }
                    }

                    curr_in_pos += static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(size_of_wasi_subscription_t);
                }

                subscriptions.emplace_back_unchecked(::std::move(tmp_subscription));
            }

            // memory_locker_guard release here
        }

        // subscriptions.size() == nsubscriptions

        if(nsubscriptions == 1u && subscriptions.front_unchecked().u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock)
        {
            // Optional blocking behaviour: if there is exactly one clock subscription,
            // we can honour its timeout by sleeping before we evaluate events. This keeps
            // the main loop simple and still allows a common "sleep"-style usage of
            // poll_oneoff. For multiple subscriptions, the function remains non-blocking.

            ::fast_io::unix_timestamp sleep_duration{};

            auto const& subscriptions_front_clock{subscriptions.front_unchecked()};
            using timestamp_integral_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;
            auto const clock_timeout_integral{static_cast<timestamp_integral_t>(subscriptions_front_clock.u.u.clock.timeout)};
            auto const clock_flags{subscriptions_front_clock.u.u.clock.flags};
            auto const clock_id{subscriptions_front_clock.u.u.clock.id};
            bool const is_abstime{(clock_flags & ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime) ==
                                  ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime};

            if(!is_abstime)
            {
                // Relative timeout: block for the requested duration.
                if(clock_timeout_integral != 0u)
                {
                    constexpr timestamp_integral_t one_billion{1'000'000'000u};

                    auto const seconds_part{clock_timeout_integral / one_billion};
                    auto const ns_rem{clock_timeout_integral % one_billion};

                    constexpr timestamp_integral_t mul_factor_ts{
                        static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / one_billion)};

                    auto const subseconds_part{ns_rem * mul_factor_ts};

                    sleep_duration.seconds = static_cast<decltype(sleep_duration.seconds)>(seconds_part);
                    sleep_duration.subseconds = static_cast<decltype(sleep_duration.subseconds)>(subseconds_part);

                    ::fast_io::this_thread::sleep_for(sleep_duration);
                }
            }
            else
            {
                // Absolute timeout: compute the remaining time and sleep at most
                // until the target is reached.
                ::fast_io::posix_clock_id posix_id;  // no initialize

                switch(clock_id)
                {
                    case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_realtime:
                    {
                        posix_id = ::fast_io::posix_clock_id::realtime;
                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_monotonic:
                    {
                        posix_id = ::fast_io::posix_clock_id::monotonic;
                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_process_cputime_id:
                    {
                        posix_id = ::fast_io::posix_clock_id::process_cputime_id;
                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_thread_cputime_id:
                    {
                        posix_id = ::fast_io::posix_clock_id::thread_cputime_id;
                        break;
                    }
                    [[unlikely]] default:
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    }
                }

                ::fast_io::unix_timestamp ts;
#if defined(UWVM_CPP_EXCEPTIONS)
                try
#endif
                {
                    ts = ::fast_io::posix_clock_gettime(posix_id);
                }
#if defined(UWVM_CPP_EXCEPTIONS)
                catch(::fast_io::error)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
#endif

                constexpr timestamp_integral_t mul_factor{static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                auto const now_integral{static_cast<timestamp_integral_t>(ts.seconds * 1'000'000'000u + ts.subseconds / mul_factor)};

                if(now_integral < clock_timeout_integral)
                {
                    auto const delta_integral{clock_timeout_integral - now_integral};

                    constexpr timestamp_integral_t one_billion{1'000'000'000u};

                    auto const seconds_part{delta_integral / one_billion};
                    auto const ns_rem{delta_integral % one_billion};

                    constexpr timestamp_integral_t mul_factor_ts{
                        static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / one_billion)};

                    auto const subseconds_part{ns_rem * mul_factor_ts};

                    sleep_duration.seconds = static_cast<decltype(sleep_duration.seconds)>(seconds_part);
                    sleep_duration.subseconds = static_cast<decltype(sleep_duration.subseconds)>(subseconds_part);

                    ::fast_io::this_thread::sleep_for(sleep_duration);
                }
            }

            {
                [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                ::uwvm2::imported::wasi::wasip1::func::wasi_event_t evt{};

                evt.userdata = subscriptions_front_clock.userdata;
                evt.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                evt.type = ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock;

                if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_data_layout())
                {
                    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                        memory,
                        out,
                        reinterpret_cast<::std::byte const*>(::std::addressof(evt)),
                        reinterpret_cast<::std::byte const*>(::std::addressof(evt)) + sizeof(evt));
                }
                else
                {
                    using userdata_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::userdata_t>;
                    using errno_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::errno_t>;
                    using eventtype_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventtype_t>;

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<userdata_underlying_t2>(
                        memory,
                        out + 0u,
                        static_cast<userdata_underlying_t2>(evt.userdata));
                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<errno_underlying_t2>(
                        memory,
                        out + 8u,
                        static_cast<errno_underlying_t2>(evt.error));
                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventtype_underlying_t2>(
                        memory,
                        out + 10u,
                        static_cast<eventtype_underlying_t2>(evt.type));

                    if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_fd_readwrite_data_layout())
                    {
                        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                            memory,
                            out + 16u,
                            reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)),
                            reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)) + sizeof(evt.u.fd_readwrite));
                    }
                    else
                    {
                        using filesize_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;
                        using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;

                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<filesize_underlying_t2>(
                            memory,
                            out + 16u + 0u,
                            static_cast<filesize_underlying_t2>(evt.u.fd_readwrite.nbytes));
                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventrwflags_underlying_t2>(
                            memory,
                            out + 16u + 8u,
                            static_cast<eventrwflags_underlying_t2>(evt.u.fd_readwrite.flags));
                    }
                }

                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(
                    memory,
                    nevents,
                    static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(1u));
            }

            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
        }
        else
        {
            // bsd kqueue -> poll -> select
            // posix poll -> select
            // dos (posix 1988) select
            // win9x ws2 select + fd nosys
            // linux epoll_wait -> poll
            // winnt 5.0+ WaitForMultipleObjectsEx

            // For those who use their own FD
            ::uwvm2::utils::container::vector<::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t*> fd_p_vector{};
            ::uwvm2::utils::container::vector<::uwvm2::utils::mutex::mutex_merely_release_guard_t> fd_release_guards_vector{};
            ::uwvm2::utils::container::unordered_flat_set<::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t*> fd_unique_set{};

            [[maybe_unused]] auto get_fd_from_wasm_fd{
                [&env, &fd_p_vector, &fd_release_guards_vector, &fd_unique_set](
                    ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd) constexpr noexcept -> ::uwvm2::imported::wasi::wasip1::abi::errno_t
                {
                    ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t* curr_wasi_fd_t_p{};
                    ::uwvm2::utils::mutex::mutex_merely_release_guard_t curr_fd_release_guard{};

                    // Prevent operations to obtain the size or perform resizing at this time.
                    // Only a lock is required when acquiring the unique pointer for the file descriptor. The lock can be released once the
                    // acquisition is complete. Since the file descriptor's location is fixed and accessed via the unique pointer,

                    auto& wasm_fd_storage{env.fd_storage};

                    // Simply acquiring data using a shared_lock
                    ::uwvm2::utils::mutex::rw_shared_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

                    // Negative states have been excluded, so the conversion result will only be positive numbers.
                    using unsigned_fd_t = ::std::make_unsigned_t<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>;
                    auto const unsigned_fd{static_cast<unsigned_fd_t>(fd)};

                    // On platforms where `size_t` is smaller than the `fd` type, this check must be added.
                    constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
                    if constexpr(::std::numeric_limits<unsigned_fd_t>::max() > size_t_max)
                    {
                        if(unsigned_fd > size_t_max) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }
                    }

                    auto const fd_opens_pos{static_cast<::std::size_t>(unsigned_fd)};

                    // The minimum value in rename_map is greater than opensize.
                    if(wasm_fd_storage.opens.size() <= fd_opens_pos)
                    {
                        // Possibly within the tree being renumbered
                        if(auto const renumber_map_iter{wasm_fd_storage.renumber_map.find(fd)}; renumber_map_iter != wasm_fd_storage.renumber_map.end())
                        {
                            curr_wasi_fd_t_p = renumber_map_iter->second.fd_p;
                        }
                        else [[unlikely]]
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
                        }
                    }
                    else
                    {
                        // The addition here is safe.
                        curr_wasi_fd_t_p = wasm_fd_storage.opens.index_unchecked(fd_opens_pos).fd_p;
                    }

                // curr_wasi_fd_t_p never nullptr
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(curr_wasi_fd_t_p == nullptr) [[unlikely]]
                    {
                        // Security issues inherent to virtual machines
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
                    }
#endif

                    // Other threads will definitely lock fds_rwlock when performing close operations (since they need to access the fd
                    // vector). If the current thread is performing fdatasync, no other thread can be executing any close operations
                    // simultaneously, eliminating any destruction issues. Therefore, acquiring the lock at this point is safe. However,
                    // the problem arises when, immediately after acquiring the lock and before releasing the manager lock and beginning fd
                    // operations, another thread executes a deletion that removes this fd. Subsequent operations by the current thread
                    // would then encounter issues. Thus, locking must occur before releasing fds_rwlock.

                    auto const [iter, inserted]{fd_unique_set.insert(curr_wasi_fd_t_p)};

                    if(inserted)
                    {
                        curr_fd_release_guard.device_p = ::std::addressof(curr_wasi_fd_t_p->fd_mutex);
                        curr_fd_release_guard.lock();
                    }
                    // else: curr_fd_release_guard.device_p = nullptr;

                    // After unlocking fds_lock, members within `wasm_fd_storage_t` can no longer be accessed or modified.

                    fd_p_vector.push_back(curr_wasi_fd_t_p);
                    fd_release_guards_vector.push_back(::std::move(curr_fd_release_guard));

                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }};

            // Collect per-subscription immediate error events (for invalid FDs, rights, etc.),
            // so that poll_oneoff can still succeed globally while reporting errors per event.
            ::uwvm2::utils::container::vector<::uwvm2::imported::wasi::wasip1::func::wasi_event_t> immediate_events{};

            [[maybe_unused]] auto push_immediate_event{[&immediate_events](::uwvm2::imported::wasi::wasip1::func::wasi_subscription_t const& sub,
                                                                           ::uwvm2::imported::wasi::wasip1::abi::errno_t err) constexpr noexcept
                                                       {
                                                           ::uwvm2::imported::wasi::wasip1::func::wasi_event_t evt{};
                                                           evt.userdata = sub.userdata;
                                                           evt.error = err;
                                                           evt.type = sub.u.tag;
                                                           evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                                                           evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                                                           immediate_events.push_back(evt);
                                                       }};

            // Shared helper to encode a wasi_event_t into guest memory at out_curr and bump produced.
            [[maybe_unused]] auto write_one_event_to_memory{
                [&memory](::uwvm2::imported::wasi::wasip1::func::wasi_event_t const& evt,
                          ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t& out_curr,
                          ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t& produced) constexpr noexcept
                {
                    if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_data_layout())
                    {
                        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                            memory,
                            out_curr,
                            reinterpret_cast<::std::byte const*>(::std::addressof(evt)),
                            reinterpret_cast<::std::byte const*>(::std::addressof(evt)) + sizeof(evt));
                    }
                    else
                    {
                        using userdata_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::userdata_t>;
                        using errno_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::errno_t>;
                        using eventtype_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventtype_t>;

                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<userdata_underlying_t2>(
                            memory,
                            out_curr + 0u,
                            static_cast<userdata_underlying_t2>(evt.userdata));
                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<errno_underlying_t2>(
                            memory,
                            out_curr + 8u,
                            static_cast<errno_underlying_t2>(evt.error));
                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventtype_underlying_t2>(
                            memory,
                            out_curr + 10u,
                            static_cast<eventtype_underlying_t2>(evt.type));

                        if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_fd_readwrite_data_layout())
                        {
                            ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                                memory,
                                out_curr + 16u,
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)),
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)) + sizeof(evt.u.fd_readwrite));
                        }
                        else
                        {
                            using filesize_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;
                            using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;

                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<filesize_underlying_t2>(
                                memory,
                                out_curr + 16u + 0u,
                                static_cast<filesize_underlying_t2>(evt.u.fd_readwrite.nbytes));
                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventrwflags_underlying_t2>(
                                memory,
                                out_curr + 16u + 8u,
                                static_cast<eventrwflags_underlying_t2>(evt.u.fd_readwrite.flags));
                        }
                    }

                    out_curr += static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(::uwvm2::imported::wasi::wasip1::func::size_of_wasi_event_t);
                    ++produced;
                }};

#if defined(__linux__)
            // linux

# if (defined(__NR_epoll_create1) || defined(__NR_epoll_create)) && defined(__NR_epoll_ctl) && defined(__NR_timerfd_create) &&                                 \
     defined(__NR_timerfd_settime) && defined(__NR_epoll_wait)
            // syscall __NR_epoll_create1 or __NR_epoll_create, __NR_epoll_ctl, __NR_timerfd_create, __NR_timerfd_settime, __NR_epoll_wait

            ::uwvm2::utils::container::vector<::fast_io::posix_file> fds{};  // RAII Close
            bool has_epoll_interest{};

            int const epfd{
#  if defined(__NR_epoll_create1)
                ::fast_io::system_call<__NR_epoll_create1, int>(EPOLL_CLOEXEC)
#  else
                ::fast_io::system_call<__NR_epoll_create, int>(1)
#  endif
            };

            if(::fast_io::linux_system_call_fails(epfd)) [[unlikely]]
            {
                ::fast_io::error fe{};
                fe.domain = ::fast_io::posix_domain_value;
                fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(-epfd));

                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
            }

            // add to raii close
            fds.push_back(::fast_io::posix_file{epfd});

            ::uwvm2::utils::container::vector<struct ::epoll_event> ep_events{};
            for(auto const& sub: subscriptions)
            {
                switch(sub.u.tag)
                {
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_read: [[fallthrough]];
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write:
                    {
                        if(auto const ret{
                               get_fd_from_wasm_fd(static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>(sub.u.u.fd_readwrite.file_descriptor))};
                           ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]]
                        {
                            push_immediate_event(sub, ret);
                            continue;
                        }

                        // curr_fd_uniptr is not null.
                        auto& curr_fd{*fd_p_vector.back_unchecked()};

                        // If obtained from the renumber map, it will always be the correct value. If obtained from the open vec, it requires checking whether
                        // it is closed. Therefore, a unified check is implemented.
                        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf);
                            continue;
                        }

                        // Rights check: poll need right_poll_fd_readwrite
                        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) !=
                           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable);
                            continue;
                        }

                        // If ptr is null, it indicates an attempt to open a closed file. However, the preceding check for close pos already prevents such
                        // closed files from
                        // being processed, making this a virtual machine implementation error.
                        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
                        {
// This will be checked at runtime.
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio);
                            continue;
                        }

                        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
                        {
                            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
                            {
                                push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio);
                                continue;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file:
                            {
                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
                            {
                                // The directory FD can be passed to poll as a valid FD, but it will never become “ready”.
                                continue;
                            }
                            [[unlikely]] default:
                            {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                                ::fast_io::fast_terminate();
                            }
                        }

                        struct ::epoll_event ev{};

                        bool const is_write{sub.u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write};
                        ev.events = is_write ? EPOLLOUT : EPOLLIN;

                        ev.data.ptr = const_cast<void*>(static_cast<void const*>(::std::addressof(sub)));

                        int ret{::fast_io::system_call<__NR_epoll_ctl, int>(epfd,
                                                                            EPOLL_CTL_ADD,
                                                                            curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.native_handle(),
                                                                            ::std::addressof(ev))};
                        if(::fast_io::linux_system_call_fails(ret)) [[unlikely]]
                        {
                            auto int err{-ret};

                            if(err == EEXIST)
                            {
                                // The same underlying fd already exists in the epoll instance.
                                // Upgrade the interest set to monitor both read and write events,
                                // so that multiple subscriptions (read and write) on the same fd
                                // will not lose notifications.
                                ev.events = EPOLLIN | EPOLLOUT;

                                ret = ::fast_io::system_call<__NR_epoll_ctl, int>(epfd,
                                                                                  EPOLL_CTL_MOD,
                                                                                  curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.native_handle(),
                                                                                  ::std::addressof(ev));
                                if(::fast_io::linux_system_call_fails(ret)) [[unlikely]]
                                {
                                    ::fast_io::error fe{};
                                    fe.domain = ::fast_io::posix_domain_value;
                                    fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(-ret));

                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
                                }
                            }
                            else
                            {
                                ::fast_io::error fe{};
                                fe.domain = ::fast_io::posix_domain_value;
                                fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(err));

                                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
                            }
                        }

                        has_epoll_interest = true;

                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock:
                    {
                        using timestamp_integral_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;

                        auto const timeout_integral{static_cast<timestamp_integral_t>(sub.u.u.clock.timeout)};
                        auto const clock_flags{sub.u.u.clock.flags};
                        auto const clock_id{sub.u.u.clock.id};
                        bool const is_abstime{(clock_flags & ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime) ==
                                              ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime};

                        timestamp_integral_t effective_timeout{};

                        if(!is_abstime) { effective_timeout = timeout_integral; }
                        else
                        {
                            ::fast_io::posix_clock_id posix_id;  // no initialize

                            switch(clock_id)
                            {
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_realtime:
                                {
                                    posix_id = ::fast_io::posix_clock_id::realtime;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_monotonic:
                                {
                                    posix_id = ::fast_io::posix_clock_id::monotonic;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_process_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::process_cputime_id;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_thread_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::thread_cputime_id;
                                    break;
                                }
                                [[unlikely]] default:
                                {
                                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                                }
                            }

                            ::fast_io::unix_timestamp ts;
#  if defined(UWVM_CPP_EXCEPTIONS)
                            try
#  endif
                            {
                                ts = ::fast_io::posix_clock_gettime(posix_id);
                            }
#  if defined(UWVM_CPP_EXCEPTIONS)
                            catch(::fast_io::error)
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            }
#  endif

                            constexpr timestamp_integral_t mul_factor{
                                static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                            auto const now_integral{static_cast<timestamp_integral_t>(ts.seconds * 1'000'000'000u + ts.subseconds / mul_factor)};

                            if(now_integral >= timeout_integral) { effective_timeout = static_cast<timestamp_integral_t>(1u); }
                            else
                            {
                                effective_timeout = timeout_integral - now_integral;
                            }
                        }

                        constexpr timestamp_integral_t one_billion{1'000'000'000u};

                        if(effective_timeout == 0u) { effective_timeout = static_cast<timestamp_integral_t>(1u); }

                        auto const seconds_part{effective_timeout / one_billion};
                        auto const ns_rem{effective_timeout % one_billion};

                        int linux_clock_id;  // no initialize

                        switch(clock_id)
                        {
                            case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_realtime:
                            {
                                linux_clock_id = CLOCK_REALTIME;
                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_monotonic:
                            {
                                linux_clock_id = CLOCK_MONOTONIC;
                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_process_cputime_id:
                            {
                                linux_clock_id = CLOCK_PROCESS_CPUTIME_ID;
                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_thread_cputime_id:
                            {
                                linux_clock_id = CLOCK_THREAD_CPUTIME_ID;
                                break;
                            }
                            [[unlikely]] default:
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                            }
                        }

                        int const tfd{::fast_io::system_call<__NR_timerfd_create, int>(linux_clock_id, TFD_NONBLOCK | TFD_CLOEXEC)};
                        if(::fast_io::linux_system_call_fails(tfd)) [[unlikely]]
                        {
                            ::fast_io::error fe{};
                            fe.domain = ::fast_io::posix_domain_value;
                            fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(-tfd));

                            return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
                        }

                        // add to raii close
                        fds.push_back(::fast_io::posix_file{tfd});

                        struct ::itimerspec ts{};
                        ts.it_value.tv_sec = static_cast<decltype(ts.it_value.tv_sec)>(seconds_part);
                        ts.it_value.tv_nsec = static_cast<decltype(ts.it_value.tv_nsec)>(ns_rem);

                        int ret{::fast_io::system_call<__NR_timerfd_settime, int>(tfd, 0, ::std::addressof(ts), nullptr)};
                        if(::fast_io::linux_system_call_fails(ret)) [[unlikely]]
                        {
                            ::fast_io::error fe{};
                            fe.domain = ::fast_io::posix_domain_value;
                            fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(-ret));

                            return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
                        }

                        struct ::epoll_event ev{};
                        ev.events = EPOLLIN;
                        ev.data.ptr = const_cast<void*>(static_cast<void const*>(::std::addressof(sub)));

                        ret = ::fast_io::system_call<__NR_epoll_ctl, int>(epfd, EPOLL_CTL_ADD, tfd, ::std::addressof(ev));
                        if(::fast_io::linux_system_call_fails(ret)) [[unlikely]]
                        {
                            ::fast_io::error fe{};
                            fe.domain = ::fast_io::posix_domain_value;
                            fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(-ret));

                            return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
                        }

                        has_epoll_interest = true;

                        break;
                    }
                    [[unlikely]] default:
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    }
                }
            }

            if(!has_epoll_interest)
            {
                ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t produced{};

                {
                    [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                    auto out_curr{out};

                    for(auto const& evt: immediate_events) { write_one_event_to_memory(evt, out_curr, produced); }

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(memory, nevents, produced);
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }

            if(subscriptions.size() > static_cast<::std::size_t>(::std::numeric_limits<int>::max()))
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
            }

            ep_events.resize(subscriptions.size());

            int ready{};

            for(;;)
            {
                ready = ::fast_io::system_call<__NR_epoll_wait, int>(epfd, ep_events.data(), static_cast<int>(ep_events.size()), -1);
                if(!::fast_io::linux_system_call_fails(ready)) { break; }

                auto int err{-ready};

                if(err == EINTR) { continue; }

                ::fast_io::error fe{};
                fe.domain = ::fast_io::posix_domain_value;
                fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(err));

                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
            }

            if(static_cast<::std::size_t>(ready) > ep_events.size()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

            ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t produced{};

            {
                [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                ::uwvm2::imported::wasi::wasip1::func::wasi_event_t evt{};

                auto out_curr{out};

                for(auto const& imm_evt: immediate_events) { write_one_event_to_memory(imm_evt, out_curr, produced); }

                auto const ep_events_begin{ep_events.cbegin()};
                auto const ep_events_end{ep_events_begin + ready};
                for(auto ep_events_curr{ep_events_begin}; ep_events_curr != ep_events_end; ++ep_events_curr)
                {
                    auto const& e{*ep_events_curr};

                    auto const sub_p{static_cast<::uwvm2::imported::wasi::wasip1::func::wasi_subscription_t const*>(e.data.ptr)};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(sub_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

                    auto const sub_tag{sub_p->u.tag};

                    bool const has_error{(e.events & EPOLLERR) != 0u};
                    auto const event_error{has_error ? ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio
                                                     : ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess};

                    if(sub_tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_read ||
                       sub_tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write)
                    {
                        auto const fd{sub_p->u.u.fd_readwrite.file_descriptor};

                        if((e.events & (EPOLLIN | EPOLLHUP | EPOLLRDHUP)) != 0u)
                        {
                            for(auto const& sub: subscriptions)
                            {
                                if(sub.u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_read &&
                                   sub.u.u.fd_readwrite.file_descriptor == fd)
                                {
                                    evt.userdata = sub.userdata;
                                    evt.error = event_error;
                                    evt.type = ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_read;

                                    evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                                    evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                                    if((e.events & (EPOLLHUP | EPOLLRDHUP)) != 0u)
                                    {
                                        using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;
                                        evt.u.fd_readwrite.flags =
                                            static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(static_cast<eventrwflags_underlying_t2>(
                                                ::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t::event_fd_readwrite_hangup));
                                    }

                                    write_one_event_to_memory(evt, out_curr, produced);
                                }
                            }
                        }

                        if((e.events & (EPOLLOUT | EPOLLHUP | EPOLLRDHUP)) != 0u)
                        {
                            for(auto const& sub: subscriptions)
                            {
                                if(sub.u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write &&
                                   sub.u.u.fd_readwrite.file_descriptor == fd)
                                {
                                    evt.userdata = sub.userdata;
                                    evt.error = event_error;
                                    evt.type = ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write;

                                    evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                                    evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                                    if((e.events & (EPOLLHUP | EPOLLRDHUP)) != 0u)
                                    {
                                        using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;
                                        evt.u.fd_readwrite.flags =
                                            static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(static_cast<eventrwflags_underlying_t2>(
                                                ::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t::event_fd_readwrite_hangup));
                                    }

                                    write_one_event_to_memory(evt, out_curr, produced);
                                }
                            }
                        }
                    }
                    else
                    {
                        evt.userdata = sub_p->userdata;
                        evt.error = event_error;
                        evt.type = sub_tag;

                        evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                        evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                        write_one_event_to_memory(evt, out_curr, produced);
                    }
                }

                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(memory, nevents, produced);
            }

            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;

# elif defined(__NR_poll)

            // syscall __NR_poll

            ::uwvm2::utils::container::vector<struct ::pollfd> poll_fds{};
            ::uwvm2::utils::container::vector<::uwvm2::imported::wasi::wasip1::func::wasi_subscription_t const*> poll_subs{};

            // Record clock subscriptions and their corresponding relative timeouts (unit: ns)
            using timestamp_integral_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;

            struct clock_sub_entry
            {
                ::uwvm2::imported::wasi::wasip1::func::wasi_subscription_t const* sub{};
                timestamp_integral_t effective_timeout_ns{};
            };

            ::uwvm2::utils::container::vector<clock_sub_entry> clock_subs{};
            bool have_clock_timeout{};
            timestamp_integral_t min_clock_timeout_ns{};

            for(auto const& sub: subscriptions)
            {
                switch(sub.u.tag)
                {
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_read: [[fallthrough]];
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write:
                    {
                        if(auto const ret{
                               get_fd_from_wasm_fd(static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>(sub.u.u.fd_readwrite.file_descriptor))};
                           ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]]
                        {
                            push_immediate_event(sub, ret);
                            continue;
                        }

                        auto& curr_fd{*fd_p_vector.back_unchecked()};

                        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf);
                            continue;
                        }

                        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) !=
                           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable);
                            continue;
                        }

                        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
                        {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio);
                            continue;
                        }

                        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
                        {
                            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
                            {
                                push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio);
                                continue;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file:
                            {
                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
                            {
                                // Directory FD is allowed to be passed in, but it will never be ready, so it is skipped here.
                                continue;
                            }
                            [[unlikely]] default:
                            {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                                ::fast_io::fast_terminate();
                            }
                        }

                        struct ::pollfd pfd{};
                        pfd.fd = curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.native_handle();

                        bool const is_write{sub.u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write};
                        pfd.events = static_cast<short>(is_write ? POLLOUT : POLLIN);
                        pfd.revents = 0;

                        poll_fds.push_back(pfd);
                        poll_subs.push_back(::std::addressof(sub));

                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock:
                    {
                        auto const timeout_integral{static_cast<timestamp_integral_t>(sub.u.u.clock.timeout)};
                        auto const clock_flags{sub.u.u.clock.flags};
                        auto const clock_id{sub.u.u.clock.id};

                        bool const is_abstime{(clock_flags & ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime) ==
                                              ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime};

                        timestamp_integral_t effective_timeout{};

                        if(!is_abstime) { effective_timeout = timeout_integral; }
                        else
                        {
                            ::fast_io::posix_clock_id posix_id;  // no init

                            switch(clock_id)
                            {
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_realtime:
                                {
                                    posix_id = ::fast_io::posix_clock_id::realtime;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_monotonic:
                                {
                                    posix_id = ::fast_io::posix_clock_id::monotonic;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_process_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::process_cputime_id;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_thread_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::thread_cputime_id;
                                    break;
                                }
                                [[unlikely]] default:
                                {
                                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                                }
                            }

                            ::fast_io::unix_timestamp ts;
#  if defined(UWVM_CPP_EXCEPTIONS)
                            try
#  endif
                            {
                                ts = ::fast_io::posix_clock_gettime(posix_id);
                            }
#  if defined(UWVM_CPP_EXCEPTIONS)
                            catch(::fast_io::error)
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            }
#  endif

                            constexpr timestamp_integral_t mul_factor{
                                static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                            auto const now_integral{static_cast<timestamp_integral_t>(ts.seconds * 1'000'000'000u + ts.subseconds / mul_factor)};

                            if(now_integral >= timeout_integral) { effective_timeout = static_cast<timestamp_integral_t>(0u); }
                            else
                            {
                                effective_timeout = timeout_integral - now_integral;
                            }
                        }

                        clock_sub_entry ce{};
                        ce.sub = ::std::addressof(sub);
                        ce.effective_timeout_ns = effective_timeout;
                        clock_subs.push_back(ce);

                        if(!have_clock_timeout)
                        {
                            min_clock_timeout_ns = effective_timeout;
                            have_clock_timeout = true;
                        }
                        else if(effective_timeout < min_clock_timeout_ns) { min_clock_timeout_ns = effective_timeout; }

                        break;
                    }
                    [[unlikely]] default:
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    }
                }
            }

            if(poll_fds.empty() && !have_clock_timeout)
            {
                ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t produced{};

                {
                    [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                    auto out_curr{out};

                    for(auto const& evt: immediate_events) { write_one_event_to_memory(evt, out_curr, produced); }

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(memory, nevents, produced);
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }

            // Calculate poll timeout (milliseconds)
            int timeout_ms{-1};

            if(have_clock_timeout)
            {
                // Round up to milliseconds to ensure it is not shorter than the expected timeout
                constexpr timestamp_integral_t ns_per_ms{1'000'000u};
                auto const added{min_clock_timeout_ns + (ns_per_ms - static_cast<timestamp_integral_t>(1u))};
                auto const ms{added / ns_per_ms};

                if(ms > static_cast<timestamp_integral_t>(::std::numeric_limits<int>::max())) { timeout_ms = ::std::numeric_limits<int>::max(); }
                else
                {
                    timeout_ms = static_cast<int>(ms);
                }
            }

            // If there are immediate events to report, avoid blocking in poll().
            if(!immediate_events.empty()) { timeout_ms = 0; }

            if(poll_fds.size() > static_cast<::std::size_t>(::std::numeric_limits<::nfds_t>::max()))
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
            }

            int const ready{::fast_io::system_call<__NR_poll, int>(poll_fds.data(), static_cast<::nfds_t>(poll_fds.size()), timeout_ms)};

            if(::fast_io::linux_system_call_fails(ready)) [[unlikely]]
            {
                ::fast_io::error fe{};
                fe.domain = ::fast_io::posix_domain_value;
                fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(-ready));

                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
            }

            ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t produced{};

            {
                [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                auto out_curr{out};

                // First flush immediate error events
                for(auto const& evt: immediate_events)
                {
                    if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_data_layout())
                    {
                        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                            memory,
                            out_curr,
                            reinterpret_cast<::std::byte const*>(::std::addressof(evt)),
                            reinterpret_cast<::std::byte const*>(::std::addressof(evt)) + sizeof(evt));
                    }
                    else
                    {
                        using userdata_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::userdata_t>;
                        using errno_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::errno_t>;
                        using eventtype_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventtype_t>;

                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<userdata_underlying_t2>(
                            memory,
                            out_curr + 0u,
                            static_cast<userdata_underlying_t2>(evt.userdata));
                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<errno_underlying_t2>(
                            memory,
                            out_curr + 8u,
                            static_cast<errno_underlying_t2>(evt.error));
                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventtype_underlying_t2>(
                            memory,
                            out_curr + 10u,
                            static_cast<eventtype_underlying_t2>(evt.type));

                        if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_fd_readwrite_data_layout())
                        {
                            ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                                memory,
                                out_curr + 16u,
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)),
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)) + sizeof(evt.u.fd_readwrite));
                        }
                        else
                        {
                            using filesize_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;
                            using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;

                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<filesize_underlying_t2>(
                                memory,
                                out_curr + 16u + 0u,
                                static_cast<filesize_underlying_t2>(evt.u.fd_readwrite.nbytes));
                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventrwflags_underlying_t2>(
                                memory,
                                out_curr + 16u + 8u,
                                static_cast<eventrwflags_underlying_t2>(evt.u.fd_readwrite.flags));
                        }
                    }

                    out_curr += static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(::uwvm2::imported::wasi::wasip1::func::size_of_wasi_event_t);
                    ++produced;
                }

                // First handle FD events
                auto pfd_curr{poll_fds.cbegin()};
                auto const pfd_end{pfd_curr + poll_fds.size()};
                auto sub_curr{poll_subs.cbegin()};

                if(poll_subs.size() < poll_fds.size()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                for(; pfd_curr != pfd_end; ++pfd_curr, ++sub_curr)
                {
                    auto const& pfd{*pfd_curr};

                    if(pfd.revents == 0) { continue; }

                    auto const sub_p{*sub_curr};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(sub_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

                    evt.userdata = sub_p->userdata;
                    evt.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                    evt.type = sub_p->u.tag;

                    evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                    evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                    if((pfd.revents & (POLLHUP | POLLERR)) != 0)
                    {
                        using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;
                        evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(
                            static_cast<eventrwflags_underlying_t2>(::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t::event_fd_readwrite_hangup));
                    }

                    if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_data_layout())
                    {
                        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                            memory,
                            out_curr,
                            reinterpret_cast<::std::byte const*>(::std::addressof(evt)),
                            reinterpret_cast<::std::byte const*>(::std::addressof(evt)) + sizeof(evt));
                    }
                    else
                    {
                        using userdata_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::userdata_t>;
                        using errno_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::errno_t>;
                        using eventtype_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventtype_t>;

                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<userdata_underlying_t2>(
                            memory,
                            out_curr + 0u,
                            static_cast<userdata_underlying_t2>(evt.userdata));
                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<errno_underlying_t2>(
                            memory,
                            out_curr + 8u,
                            static_cast<errno_underlying_t2>(evt.error));
                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventtype_underlying_t2>(
                            memory,
                            out_curr + 10u,
                            static_cast<eventtype_underlying_t2>(evt.type));

                        if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_fd_readwrite_data_layout())
                        {
                            ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                                memory,
                                out_curr + 16u,
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)),
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)) + sizeof(evt.u.fd_readwrite));
                        }
                        else
                        {
                            using filesize_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;
                            using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;

                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<filesize_underlying_t2>(
                                memory,
                                out_curr + 16u + 0u,
                                static_cast<filesize_underlying_t2>(evt.u.fd_readwrite.nbytes));
                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventrwflags_underlying_t2>(
                                memory,
                                out_curr + 16u + 8u,
                                static_cast<eventrwflags_underlying_t2>(evt.u.fd_readwrite.flags));
                        }
                    }

                    out_curr += static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(::uwvm2::imported::wasi::wasip1::func::size_of_wasi_event_t);
                    ++produced;
                }

                // Then handle clock events: only trigger those with effective_timeout_ns == min_clock_timeout_ns
                if(have_clock_timeout && ready == 0)
                {
                    for(auto const& ce: clock_subs)
                    {
                        if(ce.effective_timeout_ns != min_clock_timeout_ns) { continue; }

                        auto const sub_p{ce.sub};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(sub_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

                        auto const clock_flags{sub_p->u.u.clock.flags};
                        bool const is_abstime{(clock_flags & ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime) ==
                                              ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime};

                        if(is_abstime)
                        {
                            using timestamp_integral_t_local = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;

                            auto const timeout_integral{static_cast<timestamp_integral_t_local>(sub_p->u.u.clock.timeout)};
                            auto const clock_id{sub_p->u.u.clock.id};

                            ::fast_io::posix_clock_id posix_id;  // no init

                            switch(clock_id)
                            {
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_realtime:
                                {
                                    posix_id = ::fast_io::posix_clock_id::realtime;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_monotonic:
                                {
                                    posix_id = ::fast_io::posix_clock_id::monotonic;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_process_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::process_cputime_id;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_thread_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::thread_cputime_id;
                                    break;
                                }
                                [[unlikely]] default:
                                {
                                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                                }
                            }

                            ::fast_io::unix_timestamp ts2;
#  if defined(UWVM_CPP_EXCEPTIONS)
                            try
#  endif
                            {
                                ts2 = ::fast_io::posix_clock_gettime(posix_id);
                            }
#  if defined(UWVM_CPP_EXCEPTIONS)
                            catch(::fast_io::error)
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            }
#  endif

                            constexpr timestamp_integral_t_local mul_factor2{
                                static_cast<timestamp_integral_t_local>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                            auto const now_integral{static_cast<timestamp_integral_t_local>(ts2.seconds * 1'000'000'000u + ts2.subseconds / mul_factor2)};

                            if(now_integral < timeout_integral) { continue; }
                        }

                        evt.userdata = sub_p->userdata;
                        evt.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                        evt.type = sub_p->u.tag;

                        evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                        evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                        if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_data_layout())
                        {
                            ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                                memory,
                                out_curr,
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt)),
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt)) + sizeof(evt));
                        }
                        else
                        {
                            using userdata_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::userdata_t>;
                            using errno_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::errno_t>;
                            using eventtype_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventtype_t>;

                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<userdata_underlying_t2>(
                                memory,
                                out_curr + 0u,
                                static_cast<userdata_underlying_t2>(evt.userdata));
                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<errno_underlying_t2>(
                                memory,
                                out_curr + 8u,
                                static_cast<errno_underlying_t2>(evt.error));
                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventtype_underlying_t2>(
                                memory,
                                out_curr + 10u,
                                static_cast<eventtype_underlying_t2>(evt.type));

                            // For clock events, the u region is actually unused in the current ABI, but for compatibility,
                            // this code reuses the encoding path of fd_readwrite (nbytes/flags are already 0).
                            if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_fd_readwrite_data_layout())
                            {
                                ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                                    memory,
                                    out_curr + 16u,
                                    reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)),
                                    reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)) + sizeof(evt.u.fd_readwrite));
                            }
                            else
                            {
                                using filesize_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;
                                using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;

                                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<filesize_underlying_t2>(
                                    memory,
                                    out_curr + 16u + 0u,
                                    static_cast<filesize_underlying_t2>(evt.u.fd_readwrite.nbytes));
                                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventrwflags_underlying_t2>(
                                    memory,
                                    out_curr + 16u + 8u,
                                    static_cast<eventrwflags_underlying_t2>(evt.u.fd_readwrite.flags));
                            }
                        }

                        out_curr +=
                            static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(::uwvm2::imported::wasi::wasip1::func::size_of_wasi_event_t);
                        ++produced;
                    }
                }

                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(memory, nevents, produced);
            }

            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
# else
            // old linux
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
# endif
#elif defined(__DragonFly__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || (defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            // Newer BSD
            // kqueue()

            ::uwvm2::utils::container::vector<::fast_io::posix_file> fds{};  // RAII Close for kqueue fd

            int const kq{::uwvm2::imported::wasi::wasip1::func::posix::kqueue()};
            if(kq == -1)
            {
                ::fast_io::error fe{};
                fe.domain = ::fast_io::posix_domain_value;
                fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(errno));

                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
            }

            fds.push_back(::fast_io::posix_file{kq});

            using timestamp_integral_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;

            struct clock_sub_entry
            {
                ::uwvm2::imported::wasi::wasip1::func::wasi_subscription_t const* sub{};
                timestamp_integral_t effective_timeout_ns{};
            };

            ::uwvm2::utils::container::vector<struct ::kevent> change_list{};
            ::uwvm2::utils::container::vector<clock_sub_entry> clock_subs{};
            bool have_clock_timeout{};
            timestamp_integral_t min_clock_timeout_ns{};

            for(auto const& sub: subscriptions)
            {
                switch(sub.u.tag)
                {
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_read: [[fallthrough]];
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write:
                    {
                        if(auto const ret{
                               get_fd_from_wasm_fd(static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>(sub.u.u.fd_readwrite.file_descriptor))};
                           ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]]
                        {
                            push_immediate_event(sub, ret);
                            continue;
                        }

                        auto& curr_fd{*fd_p_vector.back_unchecked()};

                        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf);
                            continue;
                        }

                        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) !=
                           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable);
                            continue;
                        }

                        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
                        {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio);
                            continue;
                        }

                        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
                        {
                            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
                            {
                                push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio);
                                continue;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file:
                            {
                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
                            {
                                // The directory FD can be passed to poll as a valid FD, but it will never become “ready”.
                                continue;
                            }
                            [[unlikely]] default:
                            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                ::fast_io::fast_terminate();
                            }
                        }

                        struct ::kevent kev{};
                        int const native_fd{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.native_handle()};

                        bool const is_write{sub.u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write};
                        EV_SET(::std::addressof(kev),
                               static_cast<::uintptr_t>(native_fd),
                               is_write ? EVFILT_WRITE : EVFILT_READ,
                               EV_ADD | EV_ENABLE,
                               0,
                               0,
                               const_cast<void*>(static_cast<void const*>(::std::addressof(sub))));

                        change_list.push_back(kev);

                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock:
                    {
                        auto const timeout_integral{static_cast<timestamp_integral_t>(sub.u.u.clock.timeout)};
                        auto const clock_flags{sub.u.u.clock.flags};
                        auto const clock_id{sub.u.u.clock.id};

                        bool const is_abstime{(clock_flags & ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime) ==
                                              ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime};

                        timestamp_integral_t effective_timeout{};

                        if(!is_abstime) { effective_timeout = timeout_integral; }
                        else
                        {
                            ::fast_io::posix_clock_id posix_id;  // no init

                            switch(clock_id)
                            {
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_realtime:
                                {
                                    posix_id = ::fast_io::posix_clock_id::realtime;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_monotonic:
                                {
                                    posix_id = ::fast_io::posix_clock_id::monotonic;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_process_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::process_cputime_id;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_thread_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::thread_cputime_id;
                                    break;
                                }
                                [[unlikely]] default:
                                {
                                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                                }
                            }

                            ::fast_io::unix_timestamp ts;
# if defined(UWVM_CPP_EXCEPTIONS)
                            try
# endif
                            {
                                ts = ::fast_io::posix_clock_gettime(posix_id);
                            }
# if defined(UWVM_CPP_EXCEPTIONS)
                            catch(::fast_io::error)
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            }
# endif

                            constexpr timestamp_integral_t mul_factor{
                                static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                            auto const now_integral{static_cast<timestamp_integral_t>(ts.seconds * 1'000'000'000u + ts.subseconds / mul_factor)};

                            if(now_integral >= timeout_integral) { effective_timeout = static_cast<timestamp_integral_t>(1u); }
                            else
                            {
                                effective_timeout = timeout_integral - now_integral;
                            }
                        }

                        if(effective_timeout == 0u) { effective_timeout = static_cast<timestamp_integral_t>(1u); }

                        clock_sub_entry ce{};
                        ce.sub = ::std::addressof(sub);
                        ce.effective_timeout_ns = effective_timeout;
                        clock_subs.push_back(ce);

                        if(!have_clock_timeout)
                        {
                            min_clock_timeout_ns = effective_timeout;
                            have_clock_timeout = true;
                        }
                        else if(effective_timeout < min_clock_timeout_ns) { min_clock_timeout_ns = effective_timeout; }

                        break;
                    }
                    [[unlikely]] default:
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    }
                }
            }

            if(change_list.empty() && !have_clock_timeout)
            {
                ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t produced{};

                {
                    [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                    auto out_curr{out};

                    for(auto const& evt: immediate_events) { write_one_event_to_memory(evt, out_curr, produced); }

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(memory, nevents, produced);
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }

            struct ::timespec ts_timeout{};
            struct ::timespec* timeout_ptr{};

            if(have_clock_timeout)
            {
                using time_sec_integral_t = ::std::make_unsigned_t<::time_t>;
                constexpr timestamp_integral_t one_billion{1'000'000'000u};

                auto const seconds_part{min_clock_timeout_ns / one_billion};
                auto const ns_rem{min_clock_timeout_ns % one_billion};

                time_sec_integral_t const time_t_max_u{static_cast<time_sec_integral_t>(::std::numeric_limits<::time_t>::max())};

                if(seconds_part > static_cast<timestamp_integral_t>(time_t_max_u))
                {
                    ts_timeout.tv_sec = static_cast<::time_t>(time_t_max_u);
                    ts_timeout.tv_nsec = static_cast<long>(one_billion - static_cast<timestamp_integral_t>(1u));
                }
                else
                {
                    ts_timeout.tv_sec = static_cast<::time_t>(seconds_part);
                    ts_timeout.tv_nsec = static_cast<long>(ns_rem);
                }

                timeout_ptr = ::std::addressof(ts_timeout);
            }
            else
            {
                timeout_ptr = nullptr;
            }

            if(subscriptions.size() > static_cast<::std::size_t>(::std::numeric_limits<int>::max()))
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
            }

            ::uwvm2::utils::container::vector<struct ::kevent> events{};
            events.resize(subscriptions.size());

            int const max_events{static_cast<int>(events.size())};

            int const ready{::uwvm2::imported::wasi::wasip1::func::posix::kevent(kq,
                                                                                 change_list.empty() ? nullptr : change_list.data(),
                                                                                 static_cast<int>(change_list.size()),
                                                                                 events.data(),
                                                                                 max_events,
                                                                                 timeout_ptr)};

            if(ready == -1) [[unlikely]]
            {
                ::fast_io::error fe{};
                fe.domain = ::fast_io::posix_domain_value;
                fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(errno));

                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
            }

            if(static_cast<::std::size_t>(ready) > events.size()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

            ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t produced{};

            {
                [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                ::uwvm2::imported::wasi::wasip1::func::wasi_event_t evt{};

                auto out_curr{out};

                for(auto const& imm_evt: immediate_events) { write_one_event_to_memory(imm_evt, out_curr, produced); }

                if(ready > 0)
                {
                    auto const events_begin{events.cbegin()};
                    auto const events_end{events_begin + ready};

                    for(auto events_curr{events_begin}; events_curr != events_end; ++events_curr)
                    {
                        auto const& e{*events_curr};

                        auto const sub_p{static_cast<::uwvm2::imported::wasi::wasip1::func::wasi_subscription_t const*>(e.udata)};

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(sub_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                        evt.userdata = sub_p->userdata;
                        evt.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                        evt.type = sub_p->u.tag;

                        evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                        evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                        if((e.flags & EV_ERROR) != 0)
                        {
                            auto const ev_err_no{static_cast<int>(e.data)};
                            if(ev_err_no != 0)
                            {
                                ::fast_io::error fe{};
                                fe.domain = ::fast_io::posix_domain_value;
                                fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(ev_err_no));

                                evt.error = ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
                            }
                        }

                        if(evt.type == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_read ||
                           evt.type == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write)
                        {
                            if((e.flags & EV_EOF) != 0)
                            {
                                using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;
                                evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(
                                    static_cast<eventrwflags_underlying_t2>(::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t::event_fd_readwrite_hangup));
                            }
                        }

                        write_one_event_to_memory(evt, out_curr, produced);
                    }
                }
                else if(have_clock_timeout)
                {
                    for(auto const& ce: clock_subs)
                    {
                        if(ce.effective_timeout_ns != min_clock_timeout_ns) { continue; }

                        auto const sub_p{ce.sub};

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(sub_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                        auto const clock_flags{sub_p->u.u.clock.flags};
                        bool const is_abstime{(clock_flags & ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime) ==
                                              ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime};

                        if(is_abstime)
                        {
                            using timestamp_integral_t_local = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;

                            auto const timeout_integral{static_cast<timestamp_integral_t_local>(sub_p->u.u.clock.timeout)};
                            auto const clock_id{sub_p->u.u.clock.id};

                            ::fast_io::posix_clock_id posix_id;  // no init

                            switch(clock_id)
                            {
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_realtime:
                                {
                                    posix_id = ::fast_io::posix_clock_id::realtime;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_monotonic:
                                {
                                    posix_id = ::fast_io::posix_clock_id::monotonic;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_process_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::process_cputime_id;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_thread_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::thread_cputime_id;
                                    break;
                                }
                                [[unlikely]] default:
                                {
                                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                                }
                            }

                            ::fast_io::unix_timestamp ts2;
# if defined(UWVM_CPP_EXCEPTIONS)
                            try
# endif
                            {
                                ts2 = ::fast_io::posix_clock_gettime(posix_id);
                            }
# if defined(UWVM_CPP_EXCEPTIONS)
                            catch(::fast_io::error)
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            }
# endif

                            constexpr timestamp_integral_t_local mul_factor2{
                                static_cast<timestamp_integral_t_local>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                            auto const now_integral{static_cast<timestamp_integral_t_local>(ts2.seconds * 1'000'000'000u + ts2.subseconds / mul_factor2)};

                            if(now_integral < timeout_integral) { continue; }
                        }

                        evt.userdata = sub_p->userdata;
                        evt.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                        evt.type = sub_p->u.tag;

                        evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                        evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                        write_one_event_to_memory(evt, out_curr, produced);
                    }
                }

                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(memory, nevents, produced);
            }

            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;

#elif defined(_WIN32) && !defined(__CYGWIN__)
            // Windows
# if defined(_WIN32_WINDOWS)
            // Windows 9x
            // Since Win9x lacks the Win32 API encapsulation for DOS device operations, implementing DOS operations here would prevent porting to WinNT. UWVM2
            // aims to enable Win9x programs to run on WinNT as a compatibility target, hence this feature is not provided. Alternatively, you may use
            // msdos-djgpp.

            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
# else
            // Windows NT
            ::uwvm2::utils::container::vector<void*> wait_handles{};
            ::uwvm2::utils::container::vector<wasi_subscription_t const*> wait_subs{};

            // Process clock subscriptions to determine minimum timeout (ms)
            using timestamp_integral_t_nt = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;

            ::std::uint_least64_t min_timeout_ms_nt{};
            bool have_timeout_nt{};

            for(auto const& sub: subscriptions)
            {
                if(sub.u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock)
                {
                    auto const timeout_integral{static_cast<timestamp_integral_t_nt>(sub.u.u.clock.timeout)};
                    auto const clock_flags{sub.u.u.clock.flags};
                    auto const clock_id{sub.u.u.clock.id};

                    bool const is_abstime{(clock_flags & ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime) ==
                                          ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime};

                    timestamp_integral_t_nt effective_timeout_ns{};

                    if(!is_abstime) { effective_timeout_ns = timeout_integral; }
                    else
                    {
                        ::fast_io::posix_clock_id posix_id;  // no init

                        switch(clock_id)
                        {
                            case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_realtime:
                            {
                                posix_id = ::fast_io::posix_clock_id::realtime;
                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_monotonic:
                            {
                                posix_id = ::fast_io::posix_clock_id::monotonic;
                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_process_cputime_id:
                            {
                                posix_id = ::fast_io::posix_clock_id::process_cputime_id;
                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_thread_cputime_id:
                            {
                                posix_id = ::fast_io::posix_clock_id::thread_cputime_id;
                                break;
                            }
                            [[unlikely]] default:
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                            }
                        }

                        ::fast_io::unix_timestamp ts;
#  if defined(UWVM_CPP_EXCEPTIONS)
                        try
#  endif
                        {
                            ts = ::fast_io::posix_clock_gettime(posix_id);
                        }
#  if defined(UWVM_CPP_EXCEPTIONS)
                        catch(::fast_io::error)
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
#  endif

                        constexpr timestamp_integral_t_nt mul_factor_nt{
                            static_cast<timestamp_integral_t_nt>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                        auto const now_integral{static_cast<timestamp_integral_t_nt>(ts.seconds * 1'000'000'000u + ts.subseconds / mul_factor_nt)};

                        if(now_integral >= timeout_integral)
                        {
                            // Ensure a minimal non-zero timeout so the event is considered due
                            effective_timeout_ns = static_cast<timestamp_integral_t_nt>(1u);
                        }
                        else
                        {
                            effective_timeout_ns = static_cast<timestamp_integral_t_nt>(timeout_integral - now_integral);
                        }
                    }

                    ::std::uint_least64_t const timeout_ms{static_cast<::std::uint_least64_t>(effective_timeout_ns / 1000000u)};

                    if(!have_timeout_nt || timeout_ms < min_timeout_ms_nt)
                    {
                        min_timeout_ms_nt = timeout_ms;
                        have_timeout_nt = true;
                    }
                }
            }

            // Process FD subscriptions and collect handles
            for(auto const& sub: subscriptions)
            {
                switch(sub.u.tag)
                {
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_read: [[fallthrough]];
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write:
                    {
                        if(auto const ret{
                               get_fd_from_wasm_fd(static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>(sub.u.u.fd_readwrite.file_descriptor))};
                           ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]]
                        {
                            push_immediate_event(sub, ret);
                            continue;
                        }

                        auto& curr_fd{*fd_p_vector.back_unchecked()};

                        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf);
                            continue;
                        }

                        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) !=
                           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable);
                            continue;
                        }

                        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
                        {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio);
                            continue;
                        }

                        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
                        {
                            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
                            {
                                push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio);
                                continue;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file:
                            {
                                // On Windows, wasi_file_fd_t is win32_native_file_with_flags_t which has a file member
                                void* handle{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file.handle};
                                wait_handles.push_back(handle);
                                wait_subs.push_back(::std::addressof(sub));

                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
                            {
                                // Directory FD is allowed to be passed in, but it will never be ready, so it is skipped here.
                                continue;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket:
                            {
                                /// @todo
                                break;
                            }
                            [[unlikely]] default:
                            {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                                ::fast_io::fast_terminate();
                            }
                        }

                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock:
                    {
                        // Clock events are handled via timeout
                        /// @todo
                        break;
                    }
                    [[unlikely]] default:
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    }
                }
            }

            constexpr ::std::size_t max_wait_handles_nt{64uz};

            if(wait_handles.size() > max_wait_handles_nt) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup; }

            if(wait_handles.empty() && !have_timeout_nt)
            {
                ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t produced_nt{};

                {
                    [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                    auto out_curr{out};

                    for(auto const& evt: immediate_events) { write_one_event_to_memory(evt, out_curr, produced_nt); }

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(memory, nevents, produced_nt);
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }

            ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t produced_nt{};

            {
                [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                auto out_curr{out};

                for(auto const& evt: immediate_events) { write_one_event_to_memory(evt, out_curr, produced_nt); }

                if(!wait_handles.empty() || have_timeout_nt)
                {
                    constexpr bool zw_flag_nt{false};

                    if(!wait_handles.empty())
                    {
                        ::std::uint_least64_t timeout_100ns_nt{};
                        ::std::uint_least64_t* timeout_ptr_nt{};

                        if(have_timeout_nt)
                        {
                            timeout_100ns_nt = static_cast<::std::uint_least64_t>(-static_cast<::std::int_least64_t>(min_timeout_ms_nt * 10000LL));
                            timeout_ptr_nt = ::std::addressof(timeout_100ns_nt);
                        }

                        auto const wait_result_nt{
                            ::fast_io::win32::nt::nt_wait_for_multiple_objects<zw_flag_nt>(static_cast<::std::uint_least32_t>(wait_handles.size()),
                                                                                           wait_handles.data(),
                                                                                           ::fast_io::win32::nt::wait_type::WaitAny,
                                                                                           false,
                                                                                           timeout_ptr_nt)};

                        // STATUS_WAIT_0 .. STATUS_WAIT_63, STATUS_TIMEOUT
                        constexpr ::std::uint_least32_t status_wait_0_nt{0x00000000u};
                        constexpr ::std::uint_least32_t status_timeout_nt{0x00000102u};
                        constexpr ::std::uint_least32_t status_wait_63_nt{0x0000003Fu};

                        if(wait_result_nt >= status_wait_0_nt && wait_result_nt <= status_wait_63_nt)
                        {
                            ::std::size_t const index_nt{static_cast<::std::size_t>(wait_result_nt - status_wait_0_nt)};
                            if(index_nt < wait_subs.size())
                            {
                                auto const sub_p{wait_subs.index_unchecked(index_nt)};

                                wasi_event_t evt{};

                                evt.userdata = sub_p->userdata;
                                evt.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                                evt.type = sub_p->u.tag;
                                evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                                evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                                write_one_event_to_memory(evt, out_curr, produced_nt);
                            }
                        }
                        else if(wait_result_nt == status_timeout_nt)
                        {
                            if(!have_timeout_nt) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                            // Timeout: report first clock subscription
                            for(auto const& sub: subscriptions)
                            {
                                if(sub.u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock)
                                {
                                    wasi_event_t evt{};

                                    evt.userdata = sub.userdata;
                                    evt.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                                    evt.type = sub.u.tag;
                                    evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                                    evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                                    write_one_event_to_memory(evt, out_curr, produced_nt);
                                    break;
                                }
                            }
                        }
                        else
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
                    }
                    else if(have_timeout_nt)
                    {
                        // Only timeout, no FDs to wait for
                        ::std::int_least64_t timeout_100ns_nt{-static_cast<::std::int_least64_t>(min_timeout_ms_nt * 10000u)};

                        constexpr bool alertable_nt{false};
                        auto const delay_status_nt{::fast_io::win32::nt::nt_delay_execution<zw_flag_nt>(alertable_nt, ::std::addressof(timeout_100ns_nt))};

                        if(delay_status_nt != 0u) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                        for(auto const& sub: subscriptions)
                        {
                            if(sub.u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock)
                            {
                                wasi_event_t evt{};

                                evt.userdata = sub.userdata;
                                evt.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                                evt.type = sub.u.tag;
                                evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                                evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                                write_one_event_to_memory(evt, out_curr, produced_nt);
                                break;
                            }
                        }
                    }
                }

                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(memory, nevents, produced_nt);
            }

            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;

# endif
#elif defined(__unix) || defined(_XOPEN_SOURCE) || (defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE)) ||                                                     \
    (defined(BSD) || defined(_SYSTYPE_BSD) || defined(__BSD_VISIBLE)) || (defined(__MSDOS__) || defined(__DJGPP__))
            // POSIX or POSIX-like

# if !defined(_POSIX_C_SOURCE) || (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L)
            // POSIX.1-2001
            // libc: poll()

            ::uwvm2::utils::container::vector<struct ::pollfd> poll_fds{};
            ::uwvm2::utils::container::vector<::uwvm2::imported::wasi::wasip1::func::wasi_subscription_t const*> poll_subs{};

            // Record clock subscriptions and their corresponding relative timeouts (unit: ns)
            using timestamp_integral_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;

            struct clock_sub_entry
            {
                ::uwvm2::imported::wasi::wasip1::func::wasi_subscription_t const* sub{};
                timestamp_integral_t effective_timeout_ns{};
            };

            ::uwvm2::utils::container::vector<clock_sub_entry> clock_subs{};
            bool have_clock_timeout{};
            timestamp_integral_t min_clock_timeout_ns{};

            for(auto const& sub: subscriptions)
            {
                switch(sub.u.tag)
                {
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_read: [[fallthrough]];
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write:
                    {
                        if(auto const ret{
                               get_fd_from_wasm_fd(static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>(sub.u.u.fd_readwrite.file_descriptor))};
                           ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]]
                        {
                            push_immediate_event(sub, ret);
                            continue;
                        }

                        auto& curr_fd{*fd_p_vector.back_unchecked()};

                        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf);
                            continue;
                        }

                        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) !=
                           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable);
                            continue;
                        }

                        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
                        {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio);
                            continue;
                        }

                        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
                        {
                            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
                            {
                                push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio);
                                continue;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file:
                            {
                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
                            {
                                // Directory FD is allowed to be passed in, but it will never be ready, so it is skipped here.
                                continue;
                            }
                            [[unlikely]] default:
                            {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                                ::fast_io::fast_terminate();
                            }
                        }

                        struct ::pollfd pfd{};
                        pfd.fd = curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.native_handle();

                        bool const is_write{sub.u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write};
                        pfd.events = static_cast<short>(is_write ? POLLOUT : POLLIN);
                        pfd.revents = 0;

                        poll_fds.push_back(pfd);
                        poll_subs.push_back(::std::addressof(sub));

                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock:
                    {
                        auto const timeout_integral{static_cast<timestamp_integral_t>(sub.u.u.clock.timeout)};
                        auto const clock_flags{sub.u.u.clock.flags};
                        auto const clock_id{sub.u.u.clock.id};

                        bool const is_abstime{(clock_flags & ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime) ==
                                              ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime};

                        timestamp_integral_t effective_timeout{};

                        if(!is_abstime) { effective_timeout = timeout_integral; }
                        else
                        {
                            ::fast_io::posix_clock_id posix_id;  // no init

                            switch(clock_id)
                            {
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_realtime:
                                {
                                    posix_id = ::fast_io::posix_clock_id::realtime;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_monotonic:
                                {
                                    posix_id = ::fast_io::posix_clock_id::monotonic;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_process_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::process_cputime_id;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_thread_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::thread_cputime_id;
                                    break;
                                }
                                [[unlikely]] default:
                                {
                                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                                }
                            }

                            ::fast_io::unix_timestamp ts;
#  if defined(UWVM_CPP_EXCEPTIONS)
                            try
#  endif
                            {
                                ts = ::fast_io::posix_clock_gettime(posix_id);
                            }
#  if defined(UWVM_CPP_EXCEPTIONS)
                            catch(::fast_io::error)
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            }
#  endif

                            constexpr timestamp_integral_t mul_factor{
                                static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                            auto const now_integral{static_cast<timestamp_integral_t>(ts.seconds * 1'000'000'000u + ts.subseconds / mul_factor)};

                            if(now_integral >= timeout_integral) { effective_timeout = static_cast<timestamp_integral_t>(1u); }
                            else
                            {
                                effective_timeout = timeout_integral - now_integral;
                            }
                        }

                        constexpr timestamp_integral_t one_billion{1'000'000'000u};

                        if(effective_timeout == 0u) { effective_timeout = static_cast<timestamp_integral_t>(1u); }

                        clock_sub_entry ce{};
                        ce.sub = ::std::addressof(sub);
                        ce.effective_timeout_ns = effective_timeout;
                        clock_subs.push_back(ce);

                        if(!have_clock_timeout)
                        {
                            min_clock_timeout_ns = effective_timeout;
                            have_clock_timeout = true;
                        }
                        else if(effective_timeout < min_clock_timeout_ns) { min_clock_timeout_ns = effective_timeout; }

                        break;
                    }
                    [[unlikely]] default:
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    }
                }
            }

            if(poll_fds.empty() && !have_clock_timeout)
            {
                ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t produced{};

                {
                    [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                    auto out_curr{out};

                    for(auto const& evt: immediate_events) { write_one_event_to_memory(evt, out_curr, produced); }

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(memory, nevents, produced);
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }

            // Calculate poll timeout (milliseconds)
            int timeout_ms{-1};

            if(have_clock_timeout)
            {
                // Round up to milliseconds to ensure it is not shorter than the expected timeout
                constexpr timestamp_integral_t ns_per_ms{1'000'000u};
                auto const added{min_clock_timeout_ns + (ns_per_ms - static_cast<timestamp_integral_t>(1u))};
                auto const ms{added / ns_per_ms};

                if(ms > static_cast<timestamp_integral_t>(::std::numeric_limits<int>::max())) { timeout_ms = ::std::numeric_limits<int>::max(); }
                else
                {
                    timeout_ms = static_cast<int>(ms);
                }
            }

            if(poll_fds.size() > static_cast<::std::size_t>(::std::numeric_limits<::nfds_t>::max()))
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
            }

            int const ready{::uwvm2::imported::wasi::wasip1::func::posix::poll(poll_fds.data(), static_cast<::nfds_t>(poll_fds.size()), timeout_ms)};

            if(ready == -1) [[unlikely]]
            {
                ::fast_io::error fe{};
                fe.domain = ::fast_io::posix_domain_value;
                fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(errno));

                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
            }

            ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t produced{};

            {
                [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                ::uwvm2::imported::wasi::wasip1::func::wasi_event_t evt{};
                auto out_curr{out};

                // First flush immediate error events
                for(auto const& imm_evt: immediate_events) { write_one_event_to_memory(imm_evt, out_curr, produced); }

                // Then handle FD events

                auto pfd_curr{poll_fds.cbegin()};
                auto const pfd_end{pfd_curr + poll_fds.size()};
                auto sub_curr{poll_subs.cbegin()};

                if(poll_subs.size() < poll_fds.size()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                for(; pfd_curr != pfd_end; ++pfd_curr, ++sub_curr)
                {
                    auto const& pfd{*pfd_curr};

                    if(pfd.revents == 0) { continue; }

                    auto const sub_p{*sub_curr};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(sub_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

                    evt.userdata = sub_p->userdata;
                    evt.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                    evt.type = sub_p->u.tag;

                    evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                    evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                    if((pfd.revents & (POLLHUP | POLLERR)) != 0)
                    {
                        using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;
                        evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(
                            static_cast<eventrwflags_underlying_t2>(::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t::event_fd_readwrite_hangup));
                    }

                    if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_data_layout())
                    {
                        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                            memory,
                            out_curr,
                            reinterpret_cast<::std::byte const*>(::std::addressof(evt)),
                            reinterpret_cast<::std::byte const*>(::std::addressof(evt)) + sizeof(evt));
                    }
                    else
                    {
                        using userdata_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::userdata_t>;
                        using errno_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::errno_t>;
                        using eventtype_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventtype_t>;

                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<userdata_underlying_t2>(
                            memory,
                            out_curr + 0u,
                            static_cast<userdata_underlying_t2>(evt.userdata));
                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<errno_underlying_t2>(
                            memory,
                            out_curr + 8u,
                            static_cast<errno_underlying_t2>(evt.error));
                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventtype_underlying_t2>(
                            memory,
                            out_curr + 10u,
                            static_cast<eventtype_underlying_t2>(evt.type));

                        if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_fd_readwrite_data_layout())
                        {
                            ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                                memory,
                                out_curr + 16u,
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)),
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)) + sizeof(evt.u.fd_readwrite));
                        }
                        else
                        {
                            using filesize_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;
                            using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;

                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<filesize_underlying_t2>(
                                memory,
                                out_curr + 16u + 0u,
                                static_cast<filesize_underlying_t2>(evt.u.fd_readwrite.nbytes));
                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventrwflags_underlying_t2>(
                                memory,
                                out_curr + 16u + 8u,
                                static_cast<eventrwflags_underlying_t2>(evt.u.fd_readwrite.flags));
                        }
                    }

                    out_curr += static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(::uwvm2::imported::wasi::wasip1::func::size_of_wasi_event_t);
                    ++produced;
                }

                // Then handle clock events: only trigger those with effective_timeout_ns == min_clock_timeout_ns
                if(have_clock_timeout && ready == 0)
                {
                    for(auto const& ce: clock_subs)
                    {
                        if(ce.effective_timeout_ns != min_clock_timeout_ns) { continue; }

                        auto const sub_p{ce.sub};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(sub_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

                        auto const clock_flags{sub_p->u.u.clock.flags};
                        bool const is_abstime{(clock_flags & ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime) ==
                                              ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime};

                        if(is_abstime)
                        {
                            using timestamp_integral_t_local = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;

                            auto const timeout_integral{static_cast<timestamp_integral_t_local>(sub_p->u.u.clock.timeout)};
                            auto const clock_id{sub_p->u.u.clock.id};

                            ::fast_io::posix_clock_id posix_id;  // no init

                            switch(clock_id)
                            {
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_realtime:
                                {
                                    posix_id = ::fast_io::posix_clock_id::realtime;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_monotonic:
                                {
                                    posix_id = ::fast_io::posix_clock_id::monotonic;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_process_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::process_cputime_id;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_thread_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::thread_cputime_id;
                                    break;
                                }
                                [[unlikely]] default:
                                {
                                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                                }
                            }

                            ::fast_io::unix_timestamp ts2;
#  if defined(UWVM_CPP_EXCEPTIONS)
                            try
#  endif
                            {
                                ts2 = ::fast_io::posix_clock_gettime(posix_id);
                            }
#  if defined(UWVM_CPP_EXCEPTIONS)
                            catch(::fast_io::error)
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            }
#  endif

                            constexpr timestamp_integral_t_local mul_factor2{
                                static_cast<timestamp_integral_t_local>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                            auto const now_integral{static_cast<timestamp_integral_t_local>(ts2.seconds * 1'000'000'000u + ts2.subseconds / mul_factor2)};

                            if(now_integral < timeout_integral) { continue; }
                        }

                        evt.userdata = sub_p->userdata;
                        evt.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                        evt.type = sub_p->u.tag;

                        evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                        evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                        if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_data_layout())
                        {
                            ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                                memory,
                                out_curr,
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt)),
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt)) + sizeof(evt));
                        }
                        else
                        {
                            using userdata_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::userdata_t>;
                            using errno_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::errno_t>;
                            using eventtype_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventtype_t>;

                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<userdata_underlying_t2>(
                                memory,
                                out_curr + 0u,
                                static_cast<userdata_underlying_t2>(evt.userdata));
                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<errno_underlying_t2>(
                                memory,
                                out_curr + 8u,
                                static_cast<errno_underlying_t2>(evt.error));
                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventtype_underlying_t2>(
                                memory,
                                out_curr + 10u,
                                static_cast<eventtype_underlying_t2>(evt.type));

                            // For clock events, the u region is actually unused in the current ABI, but for compatibility,
                            // this code reuses the encoding path of fd_readwrite (nbytes/flags are already 0).
                            if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_fd_readwrite_data_layout())
                            {
                                ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                                    memory,
                                    out_curr + 16u,
                                    reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)),
                                    reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)) + sizeof(evt.u.fd_readwrite));
                            }
                            else
                            {
                                using filesize_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;
                                using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;

                                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<filesize_underlying_t2>(
                                    memory,
                                    out_curr + 16u + 0u,
                                    static_cast<filesize_underlying_t2>(evt.u.fd_readwrite.nbytes));
                                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventrwflags_underlying_t2>(
                                    memory,
                                    out_curr + 16u + 8u,
                                    static_cast<eventrwflags_underlying_t2>(evt.u.fd_readwrite.flags));
                            }
                        }

                        out_curr +=
                            static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(::uwvm2::imported::wasi::wasip1::func::size_of_wasi_event_t);
                        ++produced;
                    }
                }

                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(memory, nevents, produced);
            }

            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
# elif defined(_POSIX_SOURCE) || (defined(__MSDOS__) || defined(__DJGPP__)) || (defined(BSD) || defined(_SYSTYPE_BSD) || defined(__BSD_VISIBLE))
            // POSIX.1-1988/1990
            // select()

            ::uwvm2::utils::container::vector<::uwvm2::imported::wasi::wasip1::func::wasi_subscription_t const*> fd_subs{};
            ::uwvm2::utils::container::vector<int> native_fds{};

            using timestamp_integral_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;

            struct clock_sub_entry
            {
                ::uwvm2::imported::wasi::wasip1::func::wasi_subscription_t const* sub{};
                timestamp_integral_t effective_timeout_ns{};
            };

            ::uwvm2::utils::container::vector<clock_sub_entry> clock_subs{};
            bool have_clock_timeout{};
            timestamp_integral_t min_clock_timeout_ns{};

            struct ::fd_set readfds;
            struct ::fd_set writefds;

#  if UWVM_HAS_BUILTIN(__builtin_bzero)
            __builtin_bzero(::std::addressof(readfds), sizeof(readfds));
            __builtin_bzero(::std::addressof(writefds), sizeof(writefds));
#  else
            FD_ZERO(::std::addressof(readfds));
            FD_ZERO(::std::addressof(writefds));
#  endif

            int max_fd{-1};

            for(auto const& sub: subscriptions)
            {
                switch(sub.u.tag)
                {
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_read: [[fallthrough]];
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write:
                    {
                        if(auto const ret{
                               get_fd_from_wasm_fd(static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>(sub.u.u.fd_readwrite.file_descriptor))};
                           ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]]
                        {
                            push_immediate_event(sub, ret);
                            continue;
                        }

                        auto& curr_fd{*fd_p_vector.back_unchecked()};

                        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf);
                            continue;
                        }

                        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) !=
                           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable);
                            continue;
                        }

                        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
                        {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio);
                            continue;
                        }

                        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
                        {
                            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
                            {
                                push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio);
                                continue;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file:
                            {
                                break;
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
                            {
                                // Directory FD is allowed to be passed in, but it will never be ready, so it is skipped here.
                                continue;
                            }
                            [[unlikely]] default:
                            {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                                ::fast_io::fast_terminate();
                            }
                        }

                        int const native_fd{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.native_handle()};

                        // Ensure native_fd is within the representable range of fd_set/FD_* macros.
#  ifdef FD_SETSIZE
                        if(native_fd < 0 || native_fd >= FD_SETSIZE) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup);
                            continue;
                        }
#  else
                        if(native_fd < 0) [[unlikely]]
                        {
                            push_immediate_event(sub, ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf);
                            continue;
                        }
#  endif

                        bool const is_write{sub.u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write};

                        if(is_write) { FD_SET(native_fd, ::std::addressof(writefds)); }
                        else
                        {
                            FD_SET(native_fd, ::std::addressof(readfds));
                        }

                        if(native_fd > max_fd) { max_fd = native_fd; }

                        fd_subs.push_back(::std::addressof(sub));
                        native_fds.push_back(native_fd);

                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock:
                    {
                        auto const timeout_integral{static_cast<timestamp_integral_t>(sub.u.u.clock.timeout)};
                        auto const clock_flags{sub.u.u.clock.flags};
                        auto const clock_id{sub.u.u.clock.id};

                        bool const is_abstime{(clock_flags & ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime) ==
                                              ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime};

                        timestamp_integral_t effective_timeout{};

                        if(!is_abstime) { effective_timeout = timeout_integral; }
                        else
                        {
                            ::fast_io::posix_clock_id posix_id;  // no init

                            switch(clock_id)
                            {
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_realtime:
                                {
                                    posix_id = ::fast_io::posix_clock_id::realtime;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_monotonic:
                                {
                                    posix_id = ::fast_io::posix_clock_id::monotonic;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_process_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::process_cputime_id;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_thread_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::thread_cputime_id;
                                    break;
                                }
                                [[unlikely]] default:
                                {
                                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                                }
                            }

                            ::fast_io::unix_timestamp ts;
#  if defined(UWVM_CPP_EXCEPTIONS)
                            try
#  endif
                            {
                                ts = ::fast_io::posix_clock_gettime(posix_id);
                            }
#  if defined(UWVM_CPP_EXCEPTIONS)
                            catch(::fast_io::error)
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            }
#  endif

                            constexpr timestamp_integral_t mul_factor{
                                static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                            auto const now_integral{static_cast<timestamp_integral_t>(ts.seconds * 1'000'000'000u + ts.subseconds / mul_factor)};

                            if(now_integral >= timeout_integral) { effective_timeout = static_cast<timestamp_integral_t>(1u); }
                            else
                            {
                                effective_timeout = timeout_integral - now_integral;
                            }
                        }

                        constexpr timestamp_integral_t one_billion{1'000'000'000u};

                        if(effective_timeout == 0u) { effective_timeout = static_cast<timestamp_integral_t>(1u); }

                        clock_sub_entry ce{};
                        ce.sub = ::std::addressof(sub);
                        ce.effective_timeout_ns = effective_timeout;
                        clock_subs.push_back(ce);

                        if(!have_clock_timeout)
                        {
                            min_clock_timeout_ns = effective_timeout;
                            have_clock_timeout = true;
                        }
                        else if(effective_timeout < min_clock_timeout_ns) { min_clock_timeout_ns = effective_timeout; }

                        break;
                    }
                    [[unlikely]] default:
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    }
                }
            }

            if(fd_subs.empty() && !have_clock_timeout)
            {
                ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t produced{};

                {
                    [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                    auto out_curr{out};

                    for(auto const& evt: immediate_events) { write_one_event_to_memory(evt, out_curr, produced); }

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(memory, nevents, produced);
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }

            struct ::timeval tv_timeout{};
            struct ::timeval* timeout_ptr{};

            if(have_clock_timeout)
            {
                constexpr timestamp_integral_t ns_per_us{1'000u};
                constexpr timestamp_integral_t us_per_s{1'000'000u};

                auto const total_us{(min_clock_timeout_ns + (ns_per_us - static_cast<timestamp_integral_t>(1u))) / ns_per_us};
                auto const seconds_part{total_us / us_per_s};
                auto const usec_rem{total_us % us_per_s};

                using tv_sec_type = decltype(tv_timeout.tv_sec);
                using tv_usec_type = decltype(tv_timeout.tv_usec);
                constexpr tv_sec_type tv_sec_max{::std::numeric_limits<tv_sec_type>::max()};

                if(seconds_part > static_cast<timestamp_integral_t>(tv_sec_max))
                {
                    tv_timeout.tv_sec = tv_sec_max;
                    tv_timeout.tv_usec = static_cast<tv_usec_type>(us_per_s - static_cast<timestamp_integral_t>(1u));
                }
                else
                {
                    tv_timeout.tv_sec = static_cast<tv_sec_type>(seconds_part);
                    tv_timeout.tv_usec = static_cast<tv_usec_type>(usec_rem);
                }

                timeout_ptr = ::std::addressof(tv_timeout);
            }
            else
            {
                timeout_ptr = nullptr;
            }

            int nfds{max_fd + 1};

            int const ready{
                ::uwvm2::imported::wasi::wasip1::func::posix::select(nfds, ::std::addressof(readfds), ::std::addressof(writefds), nullptr, timeout_ptr)};

            if(ready == -1) [[unlikely]]
            {
                ::fast_io::error fe{};
                fe.domain = ::fast_io::posix_domain_value;
                fe.code = static_cast<::fast_io::error::value_type>(static_cast<unsigned int>(errno));

                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(fe);
            }

            ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t produced{};

            {
                [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                ::uwvm2::imported::wasi::wasip1::func::wasi_event_t evt{};
                auto out_curr{out};

                // First flush immediate error events
                for(auto const& imm_evt: immediate_events) { write_one_event_to_memory(imm_evt, out_curr, produced); }

                // Then handle FD events
                auto native_fds_curr{native_fds.cbegin()};

                if(native_fds.size() < fd_subs.size()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                for(auto const sub_p: fd_subs)
                {
                    int const native_fd{*native_fds_curr};
                    ++native_fds_curr;

                    bool const is_write{sub_p->u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write};

                    bool ready_fd{};

                    if(is_write)
                    {
                        if(FD_ISSET(native_fd, ::std::addressof(writefds))) { ready_fd = true; }
                    }
                    else
                    {
                        if(FD_ISSET(native_fd, ::std::addressof(readfds))) { ready_fd = true; }
                    }

                    if(!ready_fd) { continue; }

                    evt.userdata = sub_p->userdata;
                    evt.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                    evt.type = sub_p->u.tag;

                    evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                    evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                    if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_data_layout())
                    {
                        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                            memory,
                            out_curr,
                            reinterpret_cast<::std::byte const*>(::std::addressof(evt)),
                            reinterpret_cast<::std::byte const*>(::std::addressof(evt)) + sizeof(evt));
                    }
                    else
                    {
                        using userdata_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::userdata_t>;
                        using errno_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::errno_t>;
                        using eventtype_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventtype_t>;

                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<userdata_underlying_t2>(
                            memory,
                            out_curr + 0u,
                            static_cast<userdata_underlying_t2>(evt.userdata));
                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<errno_underlying_t2>(
                            memory,
                            out_curr + 8u,
                            static_cast<errno_underlying_t2>(evt.error));
                        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventtype_underlying_t2>(
                            memory,
                            out_curr + 10u,
                            static_cast<eventtype_underlying_t2>(evt.type));

                        if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_fd_readwrite_data_layout())
                        {
                            ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                                memory,
                                out_curr + 16u,
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)),
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)) + sizeof(evt.u.fd_readwrite));
                        }
                        else
                        {
                            using filesize_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;
                            using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;

                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<filesize_underlying_t2>(
                                memory,
                                out_curr + 16u + 0u,
                                static_cast<filesize_underlying_t2>(evt.u.fd_readwrite.nbytes));
                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventrwflags_underlying_t2>(
                                memory,
                                out_curr + 16u + 8u,
                                static_cast<eventrwflags_underlying_t2>(evt.u.fd_readwrite.flags));
                        }
                    }

                    out_curr += static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(::uwvm2::imported::wasi::wasip1::func::size_of_wasi_event_t);
                    ++produced;
                }

                // Then handle clock events: only trigger those with effective_timeout_ns == min_clock_timeout_ns
                if(have_clock_timeout && ready == 0)
                {
                    for(auto const& ce: clock_subs)
                    {
                        if(ce.effective_timeout_ns != min_clock_timeout_ns) { continue; }

                        auto const sub_p{ce.sub};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(sub_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

                        auto const clock_flags{sub_p->u.u.clock.flags};
                        bool const is_abstime{(clock_flags & ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime) ==
                                              ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime};

                        if(is_abstime)
                        {
                            using timestamp_integral_t_local = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;

                            auto const timeout_integral{static_cast<timestamp_integral_t_local>(sub_p->u.u.clock.timeout)};
                            auto const clock_id{sub_p->u.u.clock.id};

                            ::fast_io::posix_clock_id posix_id;  // no init

                            switch(clock_id)
                            {
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_realtime:
                                {
                                    posix_id = ::fast_io::posix_clock_id::realtime;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_monotonic:
                                {
                                    posix_id = ::fast_io::posix_clock_id::monotonic;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_process_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::process_cputime_id;
                                    break;
                                }
                                case ::uwvm2::imported::wasi::wasip1::abi::clockid_t::clock_thread_cputime_id:
                                {
                                    posix_id = ::fast_io::posix_clock_id::thread_cputime_id;
                                    break;
                                }
                                [[unlikely]] default:
                                {
                                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                                }
                            }

                            ::fast_io::unix_timestamp ts2;
#  if defined(UWVM_CPP_EXCEPTIONS)
                            try
#  endif
                            {
                                ts2 = ::fast_io::posix_clock_gettime(posix_id);
                            }
#  if defined(UWVM_CPP_EXCEPTIONS)
                            catch(::fast_io::error)
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            }
#  endif

                            constexpr timestamp_integral_t_local mul_factor2{
                                static_cast<timestamp_integral_t_local>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                            auto const now_integral{static_cast<timestamp_integral_t_local>(ts2.seconds * 1'000'000'000u + ts2.subseconds / mul_factor2)};

                            if(now_integral < timeout_integral) { continue; }
                        }

                        evt.userdata = sub_p->userdata;
                        evt.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                        evt.type = sub_p->u.tag;

                        evt.u.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                        evt.u.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                        if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_data_layout())
                        {
                            ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                                memory,
                                out_curr,
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt)),
                                reinterpret_cast<::std::byte const*>(::std::addressof(evt)) + sizeof(evt));
                        }
                        else
                        {
                            using userdata_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::userdata_t>;
                            using errno_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::errno_t>;
                            using eventtype_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventtype_t>;

                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<userdata_underlying_t2>(
                                memory,
                                out_curr + 0u,
                                static_cast<userdata_underlying_t2>(evt.userdata));
                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<errno_underlying_t2>(
                                memory,
                                out_curr + 8u,
                                static_cast<errno_underlying_t2>(evt.error));
                            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventtype_underlying_t2>(
                                memory,
                                out_curr + 10u,
                                static_cast<eventtype_underlying_t2>(evt.type));

                            // For clock events, the u region is actually unused in the current ABI, but for compatibility,
                            // this code reuses the encoding path of fd_readwrite (nbytes/flags are already 0).
                            if constexpr(::uwvm2::imported::wasi::wasip1::func::is_default_wasi_event_fd_readwrite_data_layout())
                            {
                                ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked_unlocked(
                                    memory,
                                    out_curr + 16u,
                                    reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)),
                                    reinterpret_cast<::std::byte const*>(::std::addressof(evt.u.fd_readwrite)) + sizeof(evt.u.fd_readwrite));
                            }
                            else
                            {
                                using filesize_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;
                                using eventrwflags_underlying_t2 = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>;

                                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<filesize_underlying_t2>(
                                    memory,
                                    out_curr + 16u + 0u,
                                    static_cast<filesize_underlying_t2>(evt.u.fd_readwrite.nbytes));
                                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<eventrwflags_underlying_t2>(
                                    memory,
                                    out_curr + 16u + 8u,
                                    static_cast<eventrwflags_underlying_t2>(evt.u.fd_readwrite.flags));
                            }
                        }

                        out_curr +=
                            static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(::uwvm2::imported::wasi::wasip1::func::size_of_wasi_event_t);
                        ++produced;
                    }
                }

                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(memory, nevents, produced);
            }

            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
# else
            // old unix
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
# endif
#else
            // unknown os
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#endif
        }

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
    }
}

