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
# endif
// import
# include <fast_io.h>
# include <fast_io_device.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include <uwvm2/utils/debug/impl.h>
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
        if(nsubscriptions == 0u)
        {
            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory,
                                                                                            nevents,
                                                                                            static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(0u));

            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
        }

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
        }
        else
        {
            // bsd kqueue -> poll -> select
            // posix poll -> select
            // dos (posix 1988) select
            // win9x ws2 select + fd nosys
            // linux epoll_wait -> poll
            // winnt 5.0+ WaitForMultipleObjectsEx
        }

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
    }
}

