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

    struct alignas(8uz) wasi_event_t
    {
        ::uwvm2::imported::wasi::wasip1::abi::userdata_t userdata;
        ::uwvm2::imported::wasi::wasip1::abi::errno_t error;
        ::uwvm2::imported::wasi::wasip1::abi::eventtype_t type;
        // padding to keep the following union 8-byte aligned
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8 unused0{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16 unused1{};
        wasi_event_fd_readwrite_t fd_readwrite;
    };

    inline constexpr ::std::size_t size_of_wasi_event_t{32uz};

    inline consteval bool is_default_wasi_event_data_layout() noexcept
    {
        return __builtin_offsetof(wasi_event_t, userdata) == 0uz && __builtin_offsetof(wasi_event_t, error) == 8uz &&
               __builtin_offsetof(wasi_event_t, type) == 10uz && __builtin_offsetof(wasi_event_t, fd_readwrite) == 16uz &&
               sizeof(wasi_event_t) == size_of_wasi_event_t && alignof(wasi_event_t) == 8uz && ::std::endian::native == ::std::endian::little;
    }

    struct alignas(8uz) wasi_subscription_clock_t
    {
        ::uwvm2::imported::wasi::wasip1::abi::clockid_t id;
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t timeout;
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t precision;
        ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t flags;
    };

    struct alignas(4uz) wasi_subscription_fd_readwrite_t
    {
        ::uwvm2::imported::wasi::wasip1::abi::fd_t file_descriptor;
    };

    struct alignas(8uz) wasi_subscription_u_t
    {
        ::uwvm2::imported::wasi::wasip1::abi::eventtype_t tag;
        // padding to make the union payload 8-byte aligned
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8 unused0{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16 unused1{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 unused2{};

        union payload_u
        {
            wasi_subscription_clock_t clock;
            wasi_subscription_fd_readwrite_t fd_readwrite;
        } u{};
    };

    inline constexpr ::std::size_t size_of_wasi_subscription_u_t{40uz};

    inline consteval bool is_default_wasi_subscription_u_data_layout() noexcept
    {
        return __builtin_offsetof(wasi_subscription_u_t, tag) == 0uz && __builtin_offsetof(wasi_subscription_u_t, u) == 8uz &&
               sizeof(wasi_subscription_u_t) == size_of_wasi_subscription_u_t && alignof(wasi_subscription_u_t) == 8uz &&
               ::std::endian::native == ::std::endian::little;
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
    inline ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t poll_oneoff_wasm64(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t in,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t out,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t nsubscriptions,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t nevents) noexcept
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
            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(
                memory,
                nevents,
                static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t>(0u));

            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
        }

        // Check memory bounds for the input and output arrays.
        // This protects against overflow when multiplying by the element size.
        constexpr ::std::size_t max_size_t{::std::numeric_limits<::std::size_t>::max()};

        constexpr ::std::size_t max_size_t_div_size_of_wasi_subscription_t{max_size_t / size_of_wasi_subscription_t};
        if constexpr(::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t>::max() > max_size_t_div_size_of_wasi_subscription_t)
        {
            if(nsubscriptions > max_size_t_div_size_of_wasi_subscription_t) [[unlikely]]
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eoverflow;
            }
        }

        auto const subs_bytes{static_cast<::std::size_t>(nsubscriptions) * size_of_wasi_subscription_t};

        constexpr ::std::size_t max_size_t_div_size_of_wasi_event_t{max_size_t / size_of_wasi_event_t};
        if constexpr(::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t>::max() > max_size_t_div_size_of_wasi_event_t)
        {
            if(nsubscriptions > max_size_t_div_size_of_wasi_event_t) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eoverflow; }
        }

        auto const events_bytes{static_cast<::std::size_t>(nsubscriptions) * size_of_wasi_event_t};

        ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm64(memory, in, subs_bytes);
        ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm64(memory, out, events_bytes);

        // Optional blocking behaviour: if there is exactly one clock subscription,
        // we can honour its timeout by sleeping before we evaluate events. This keeps
        // the main loop simple and still allows a common "sleep"-style usage of
        // poll_oneoff. For multiple subscriptions, the function remains non-blocking.
        if(nsubscriptions == 1u)
        {
            bool should_sleep{};
            ::fast_io::unix_timestamp sleep_duration{};

            {
                [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                wasi_subscription_t sub{};
                auto scan_in{in};

                if constexpr(is_default_wasi_subscription_data_layout())
                {
                    ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm64_unchecked_unlocked(
                        memory,
                        scan_in,
                        reinterpret_cast<::std::byte*>(::std::addressof(sub)),
                        reinterpret_cast<::std::byte*>(::std::addressof(sub)) + sizeof(sub));
                }
                else
                {
                    // Fallback: read userdata and tag; clock payload will be read below if needed.
                    sub.userdata =
                        static_cast<decltype(sub.userdata)>(::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                                                            ::uwvm2::imported::wasi::wasip1::abi::userdata_t>(memory, scan_in + 0u));

                    sub.u.tag =
                        static_cast<decltype(sub.u.tag)>(::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                                                         ::uwvm2::imported::wasi::wasip1::abi::eventtype_t>(memory, scan_in + 8u));
                }

                if(sub.u.tag == ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock)
                {
                    ::uwvm2::imported::wasi::wasip1::abi::clockid_t clock_id{};
                    ::uwvm2::imported::wasi::wasip1::abi::timestamp_t timeout{};
                    [[maybe_unused]] ::uwvm2::imported::wasi::wasip1::abi::timestamp_t precision{};
                    ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t flags{};

                    if constexpr(is_default_wasi_subscription_data_layout())
                    {
                        clock_id = sub.u.u.clock.id;
                        timeout = sub.u.u.clock.timeout;
                        precision = sub.u.u.clock.precision;
                        flags = sub.u.u.clock.flags;
                    }
                    else
                    {
                        auto const base{static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>(in + 0u * size_of_wasi_subscription_t + 16u)};

                        clock_id = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                            ::uwvm2::imported::wasi::wasip1::abi::clockid_t>(memory, base + 0u);

                        timeout = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                            ::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(memory, base + 8u);

                        precision = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                            ::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(memory, base + 16u);

                        flags = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                            ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t>(memory, base + 24u);
                    }

                    using timestamp_integral_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;
                    auto const timeout_integral{static_cast<timestamp_integral_t>(timeout)};

                    using subclockflags_integral_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::subclockflags_t>;
                    auto const flags_integral{static_cast<subclockflags_integral_t>(flags)};
                    bool const is_abstime{(flags_integral & static_cast<subclockflags_integral_t>(
                                                                ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime)) != 0};

                    if(!is_abstime)
                    {
                        // Relative timeout: block for the requested duration.
                        if(timeout_integral != 0u)
                        {
                            constexpr timestamp_integral_t one_billion{1'000'000'000u};

                            auto const seconds_part{timeout_integral / one_billion};
                            auto const ns_rem{timeout_integral % one_billion};

                            constexpr timestamp_integral_t mul_factor_ts{
                                static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / one_billion)};

                            auto const subseconds_part{ns_rem * mul_factor_ts};

                            sleep_duration.seconds = static_cast<decltype(sleep_duration.seconds)>(seconds_part);
                            sleep_duration.subseconds = static_cast<decltype(sleep_duration.subseconds)>(subseconds_part);

                            should_sleep = true;
                        }
                    }
                    else
                    {
                        // Absolute timeout: compute the remaining time and sleep at most
                        // until the target is reached.
                        ::fast_io::posix_clock_id posix_id;  // no initialize
                        bool clock_id_ok{true};

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
                                clock_id_ok = false;
                                break;
                            }
                        }

                        if(clock_id_ok)
                        {
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
                                // If the host does not support this clock, fall back to
                                // non-blocking behaviour for this call.
                                clock_id_ok = false;
                            }
#endif

                            if(clock_id_ok)
                            {
                                constexpr timestamp_integral_t mul_factor{
                                    static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                                auto const now_integral{static_cast<timestamp_integral_t>(ts.seconds * 1'000'000'000u + ts.subseconds / mul_factor)};

                                if(now_integral < timeout_integral)
                                {
                                    auto const delta_integral{timeout_integral - now_integral};

                                    constexpr timestamp_integral_t one_billion{1'000'000'000u};

                                    auto const seconds_part{delta_integral / one_billion};
                                    auto const ns_rem{delta_integral % one_billion};

                                    constexpr timestamp_integral_t mul_factor_ts{
                                        static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / one_billion)};

                                    auto const subseconds_part{ns_rem * mul_factor_ts};

                                    sleep_duration.seconds = static_cast<decltype(sleep_duration.seconds)>(seconds_part);
                                    sleep_duration.subseconds = static_cast<decltype(sleep_duration.subseconds)>(subseconds_part);

                                    should_sleep = true;
                                }
                            }
                        }
                    }
                }
            }

            if(should_sleep) { ::fast_io::this_thread::sleep_for(sleep_duration); }
        }

        // Prepare to iterate over subscriptions, reading them one-by-one from linear memory
        // under the memory lock.

        ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t events_written{};

        auto& wasm_fd_storage{env.fd_storage};

        // Local helper to push an event into WASM memory.
        auto const push_event{
            [&memory, out, &events_written](wasi_event_t const& ev) noexcept
            {
                auto const dst_off{static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>(out + events_written * size_of_wasi_event_t)};

                if constexpr(is_default_wasi_event_data_layout())
                {
                    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64_unchecked(memory,
                                                                                                  dst_off,
                                                                                                  reinterpret_cast<::std::byte const*>(::std::addressof(ev)),
                                                                                                  reinterpret_cast<::std::byte const*>(::std::addressof(ev)) +
                                                                                                      sizeof(ev));
                }
                else
                {
                    // Fallback path: store each field individually using WASI-defined offsets.
                    auto const base{dst_off};

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64_unchecked(
                        memory,
                        base + 0u,
                        static_cast<::std::underlying_type_t<decltype(ev.userdata)>>(ev.userdata));

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64_unchecked(
                        memory,
                        base + 8u,
                        static_cast<::std::underlying_type_t<decltype(ev.error)>>(ev.error));

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64_unchecked(
                        memory,
                        base + 10u,
                        static_cast<::std::underlying_type_t<decltype(ev.type)>>(ev.type));

                    // fd_readwrite.nbytes at offset 16, flags at 24
                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64_unchecked(
                        memory,
                        base + 16u,
                        static_cast<::std::underlying_type_t<decltype(ev.fd_readwrite.nbytes)>>(ev.fd_readwrite.nbytes));

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64_unchecked(
                        memory,
                        base + 24u,
                        static_cast<::std::underlying_type_t<decltype(ev.fd_readwrite.flags)>>(ev.fd_readwrite.flags));
                }

                ++events_written;
            }};

        // Acquire memory lock while we are reading subscription structures.
        {
            [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

            auto curr_in{in};

            for(::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t i{}; i != nsubscriptions; ++i)
            {
                wasi_subscription_t sub{};

                if constexpr(is_default_wasi_subscription_data_layout())
                {
                    ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm64_unchecked_unlocked(
                        memory,
                        curr_in,
                        reinterpret_cast<::std::byte*>(::std::addressof(sub)),
                        reinterpret_cast<::std::byte*>(::std::addressof(sub)) + sizeof(sub));
                    curr_in += static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>(size_of_wasi_subscription_t);
                }
                else
                {
                    // Fallback: read userdata (u64) then the union contents at the WASI-defined offsets.
                    sub.userdata =
                        static_cast<decltype(sub.userdata)>(::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                                                            ::uwvm2::imported::wasi::wasip1::abi::userdata_t>(memory, curr_in + 0u));

                    // tag
                    sub.u.tag =
                        static_cast<decltype(sub.u.tag)>(::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                                                         ::uwvm2::imported::wasi::wasip1::abi::eventtype_t>(memory, curr_in + 8u));

                    // clock payload at offset 16, fd_readwrite at same offset but smaller type
                    // We conservatively read the clock payload and fd payload separately as needed below.
                    // Advance pointer by the full struct size.
                    curr_in += static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>(size_of_wasi_subscription_t);
                }

                wasi_event_t ev{};
                ev.userdata = sub.userdata;
                ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                ev.type = sub.u.tag;
                ev.fd_readwrite.nbytes = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(0u);
                ev.fd_readwrite.flags = static_cast<::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t>(0u);

                switch(sub.u.tag)
                {
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_clock:
                    {
                        // A future, fully featured implementation may block until the timeout
                        // expires. The current implementation remains non-blocking: it
                        // generates an event immediately if the timeout has already expired
                        // (for absolute timeouts) or if the relative timeout is zero, and
                        // otherwise leaves the subscription pending.

                        ::uwvm2::imported::wasi::wasip1::abi::clockid_t clock_id{};
                        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t timeout{};
                        [[maybe_unused]] ::uwvm2::imported::wasi::wasip1::abi::timestamp_t precision{};
                        ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t flags{};

                        if constexpr(is_default_wasi_subscription_data_layout())
                        {
                            clock_id = sub.u.u.clock.id;
                            timeout = sub.u.u.clock.timeout;
                            precision = sub.u.u.clock.precision;
                            flags = sub.u.u.clock.flags;
                        }
                        else
                        {
                            auto const base{
                                static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>(in + i * size_of_wasi_subscription_t + 16u)};

                            clock_id = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                                ::uwvm2::imported::wasi::wasip1::abi::clockid_t>(memory, base + 0u);

                            timeout = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                                ::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(memory, base + 8u);

                            precision = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                                ::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(memory, base + 16u);

                            flags = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                                ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t>(memory, base + 24u);
                        }

                        using timestamp_integral_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>;
                        auto const timeout_integral{static_cast<timestamp_integral_t>(timeout)};

                        using subclockflags_integral_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::subclockflags_t>;
                        auto const flags_integral{static_cast<subclockflags_integral_t>(flags)};
                        bool const is_abstime{(flags_integral & static_cast<subclockflags_integral_t>(
                                                                    ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t::subscription_clock_abstime)) != 0};

                        // For the general multi-subscription case we do not honour non-zero
                        // relative timeouts here; callers can re-invoke poll_oneoff after
                        // some time. When there is exactly one subscription, a blocking
                        // sleep may already have been performed above, so in that case we
                        // fall through and treat the clock as ready.
                        if(!is_abstime && timeout_integral != 0u && nsubscriptions != 1u) { break; }

                        ::fast_io::posix_clock_id posix_id;  // no initialize
                        bool clock_id_ok{true};

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
                                ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                                push_event(ev);
                                clock_id_ok = false;
                                break;
                            }
                        }

                        if(!clock_id_ok) { break; }

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
                            ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                            push_event(ev);
                            break;
                        }
#endif

                        constexpr timestamp_integral_t mul_factor{
                            static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

                        auto const now_integral{static_cast<timestamp_integral_t>(ts.seconds * 1'000'000'000u + ts.subseconds / mul_factor)};

                        if(is_abstime)
                        {
                            if(now_integral < timeout_integral) { break; }
                        }
                        // else: relative timeout == 0 -> ready immediately.

                        ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                        push_event(ev);

                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_read:
                    {
                        [[fallthrough]];
                    }
                    case ::uwvm2::imported::wasi::wasip1::abi::eventtype_t::eventtype_fd_write:
                    {
                        // Read fd payload (file_descriptor)
                        ::uwvm2::imported::wasi::wasip1::abi::fd_t sub_fd_wasi{};

                        if constexpr(is_default_wasi_subscription_data_layout()) { sub_fd_wasi = sub.u.u.fd_readwrite.file_descriptor; }
                        else
                        {
                            auto const base{
                                static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>(in + i * size_of_wasi_subscription_t + 16u)};
                            sub_fd_wasi = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<
                                ::uwvm2::imported::wasi::wasip1::abi::fd_t>(memory, base + 0u);
                        }

                        // Convert WASI fd (u32) to host wasi_posix_fd_t (i32) with bounds check.
                        using wasi_fd_underlying_t = ::std::underlying_type_t<decltype(sub_fd_wasi)>;
                        auto const sub_fd_val{static_cast<wasi_fd_underlying_t>(sub_fd_wasi)};

                        if(sub_fd_val > static_cast<wasi_fd_underlying_t>(::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>::max()))
                        {
                            ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
                            push_event(ev);
                            continue;
                        }

                        auto const fd_posix{static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>(sub_fd_val)};

                        if(fd_posix < 0)
                        {
                            ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
                            push_event(ev);
                            continue;
                        }

                        // Acquire fd entry similarly to fd_read / fd_write / path_unlink_file.

                        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t* curr_wasi_fd_t_p{};
                        ::uwvm2::utils::mutex::mutex_merely_release_guard_t curr_fd_release_guard{};

                        {
                            ::uwvm2::utils::mutex::rw_shared_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

                            using unsigned_fd_t = ::std::make_unsigned_t<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>;
                            auto const unsigned_fd{static_cast<unsigned_fd_t>(fd_posix)};

                            constexpr auto size_t_max2{::std::numeric_limits<::std::size_t>::max()};
                            if constexpr(::std::numeric_limits<unsigned_fd_t>::max() > size_t_max2)
                            {
                                if(unsigned_fd > size_t_max2) [[unlikely]]
                                {
                                    ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
                                    push_event(ev);
                                    continue;
                                }
                            }

                            auto const fd_opens_pos{static_cast<::std::size_t>(unsigned_fd)};

                            if(wasm_fd_storage.opens.size() <= fd_opens_pos)
                            {
                                if(auto const renumber_map_iter{wasm_fd_storage.renumber_map.find(fd_posix)};
                                   renumber_map_iter != wasm_fd_storage.renumber_map.end())
                                {
                                    curr_wasi_fd_t_p = renumber_map_iter->second.fd_p;
                                }
                                else [[unlikely]]
                                {
                                    ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
                                    push_event(ev);
                                    continue;
                                }
                            }
                            else
                            {
                                curr_wasi_fd_t_p = wasm_fd_storage.opens.index_unchecked(fd_opens_pos).fd_p;
                            }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            if(curr_wasi_fd_t_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                            curr_fd_release_guard.device_p = ::std::addressof(curr_wasi_fd_t_p->fd_mutex);
                            curr_fd_release_guard.lock();
                        }

                        auto& curr_fd{*curr_wasi_fd_t_p};

                        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]]
                        {
                            ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
                            push_event(ev);
                            continue;
                        }

                        // Rights check for polling.
                        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) !=
                           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_poll_fd_readwrite) [[unlikely]]
                        {
                            ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable;
                            push_event(ev);
                            continue;
                        }

                        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            push_event(ev);
                            continue;
                        }

                        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
                        {
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file:
                            {
                                [[fallthrough]];
                            }
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
                            {
                                // For the initial implementation, treat files and directories as
                                // immediately ready. nbytes=0, flags=0.
                                ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                                push_event(ev);
                                break;
                            }
#if defined(_WIN32) && !defined(__CYGWIN__)
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket:
                            {
                                // Sockets are not yet supported by this poll implementation.
                                ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                                push_event(ev);
                                break;
                            }
#endif
                            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
                            {
                                [[fallthrough]];
                            }
                            default:
                            {
                                ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                                push_event(ev);
                                break;
                            }
                        }

                        break;
                    }
                    [[unlikely]] default:
                    {
                        // Unknown tag: treat as not supported.
                        ev.error = ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                        push_event(ev);
                        break;
                    }
                }
            }
        }

        // Write back the number of events we actually stored.
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(
            memory,
            nevents,
            static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t>(events_written));

        return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess;
    }
}

