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
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/object/memory/linear/impl.h>
# include <uwvm2/imported/wasi/wasip1/abi/impl.h>
# include <uwvm2/imported/wasi/wasip1/fd_manager/impl.h>
# include <uwvm2/imported/wasi/wasip1/memory/impl.h>
# include <uwvm2/imported/wasi/wasip1/environment/impl.h>
#endif

#ifndef UWVM_CPP_EXCEPTIONS
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wcpp"
# elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcpp"
# endif
# warning "Without enabling C++ exceptions, using this WASI function may cause termination."
# if defined(__clang__)
#  pragma clang diagnostic pop
# elif defined(__GNUC__)
#  pragma GCC diagnostic pop
# endif
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_WASM64)

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{

    /// @brief     WasiPreview1.clock_res_get_wasm64
    /// @details   __wasi_errno_t wasi_clock_res_get(__wasi_clockid_t clock_id, __wasi_timestamp_t* resolution);

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t clock_res_get_wasm64(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::clockid_wasm64_t clock_id,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t resolution_ptrsz) noexcept
    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(env.wasip1_memory == nullptr) [[unlikely]]
        {
            // Security issues inherent to virtual machines
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
# endif
        auto& memory{*env.wasip1_memory};

        auto const trace_wasip1_call{env.trace_wasip1_call};

        if(trace_wasip1_call) [[unlikely]]
        {
# ifdef UWVM
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"wasip1: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"clock_res_get_wasm64",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(clock_id)>>>(clock_id),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(resolution_ptrsz),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: clock_res_get_wasm64(",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(clock_id)>>>(clock_id),
                                u8", ",
                                ::fast_io::mnp::addrvw(resolution_ptrsz),
                                u8") (wasi-trace)\n");
# endif
        }

        ::fast_io::posix_clock_id id;  // no initialize
        switch(clock_id)
        {
            case ::uwvm2::imported::wasi::wasip1::abi::clockid_wasm64_t::clock_realtime:
            {
                id = ::fast_io::posix_clock_id::realtime;
                break;
            }
            case ::uwvm2::imported::wasi::wasip1::abi::clockid_wasm64_t::clock_monotonic:
            {
                id = ::fast_io::posix_clock_id::monotonic;
                break;
            }
            case ::uwvm2::imported::wasi::wasip1::abi::clockid_wasm64_t::clock_process_cputime_id:
            {
                id = ::fast_io::posix_clock_id::process_cputime_id;
                break;
            }
            case ::uwvm2::imported::wasi::wasip1::abi::clockid_wasm64_t::clock_thread_cputime_id:
            {
                id = ::fast_io::posix_clock_id::thread_cputime_id;
                break;
            }
            [[unlikely]] default:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
            }
        }

        ::fast_io::unix_timestamp ts;  // no initialize
# ifdef UWVM_CPP_EXCEPTIONS
        try
# endif
        {
            ts = ::fast_io::posix_clock_getres(id);
        }
# ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
            // This may be an unsupported system call or an ID not recognized by the system call. In this case, it is uniformly treated as an unsupported ID.
            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsup;
        }
# endif

        using timestamp_integral_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t>;

        // fast_io internally scales tv_nsec to a base of "10^19 subseconds per second".
        constexpr timestamp_integral_t mul_factor{static_cast<timestamp_integral_t>(::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u)};

        // reduced to nanoseconds
        // Since fast_io directly obtains the clock value via clock_getres, this operation will not overflow and will not produce negative values.
        auto const ts_integral{static_cast<timestamp_integral_t>(ts.seconds * 1'000'000'000u + ts.subseconds / mul_factor)};

        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, resolution_ptrsz, ts_integral);

        return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess;
    }
}  // namespace uwvm2::imported::wasi::wasip1::func

#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif
