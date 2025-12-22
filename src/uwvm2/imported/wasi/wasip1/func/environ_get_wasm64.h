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
# warning "Without enabling C++ exceptions, using this WASI function may cause termination."
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#ifdef UWVM_IMPORT_WASI_WASIP1

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{
    /// @brief     WasiPreview1.environ_get_wasm64
    /// @details   __wasi_errno_t environ_get(char **environ, char *environ_buf);
    /// @note      The WASI specification does not require implementation-side isolation of environ/environ_buf memory order; the caller must ensure they do
    ///            not overlap. Memory write visibility is guaranteed by function call synchronization boundaries.

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t environ_get_wasm64(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t environ_ptrsz,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t environ_buf_ptrsz) noexcept
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
                                u8"environ_get_wasm64",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(environ_ptrsz),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(environ_buf_ptrsz),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: environ_get_wasm64(",
                                ::fast_io::mnp::addrvw(environ_ptrsz),
                                u8", ",
                                ::fast_io::mnp::addrvw(environ_buf_ptrsz),
                                u8") (wasi-trace)\n");
# endif
        }

        auto const environ_vec_size{env.envs.size()};
        if(environ_vec_size > ::std::numeric_limits<::std::size_t>::max() / sizeof(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t) - 1uz)
            [[unlikely]]
        {
            // This is an error specific to env itself, which triggers a direct trap.
            ::fast_io::fast_terminate();
        }

        // An additional null pointer check is required.
        auto const environ_vec_size_bytes{(environ_vec_size + 1uz) * sizeof(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t)};
        ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm64(memory, environ_ptrsz, environ_vec_size_bytes);

        // Check only once to avoid excessive locking overhead.
        ::std::size_t curr_environ_size_bytes{};
        for(auto const curr_environ: env.envs)
        {
            auto const curr_environ_size{curr_environ.size()};
            if(::std::numeric_limits<::std::size_t>::max() - 1uz - curr_environ_size_bytes < curr_environ_size) [[unlikely]]
            {
                // This is an error specific to env itself, which triggers a direct trap.
                ::fast_io::fast_terminate();
            }
            // never overflow
            curr_environ_size_bytes += curr_environ_size + 1uz;  // end zero-byte
        }
        ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm64(memory, environ_buf_ptrsz, curr_environ_size_bytes);

        auto environ_curr_size{environ_ptrsz};
        auto environ_buff_curr_size{environ_buf_ptrsz};
        for(auto const curr_environ: env.envs)
        {
            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64_unchecked<
                ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>(memory, environ_curr_size, environ_buff_curr_size);
            environ_curr_size += sizeof(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t);

            ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64_unchecked(memory,
                                                                                          environ_buff_curr_size,
                                                                                          reinterpret_cast<::std::byte const*>(curr_environ.cbegin()),
                                                                                          reinterpret_cast<::std::byte const*>(curr_environ.cend()));
            environ_buff_curr_size += curr_environ.size();

            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64_unchecked<char8_t>(memory, environ_buff_curr_size, u8'\0');
            environ_buff_curr_size += sizeof(char8_t);
            static_assert(sizeof(char8_t) == 1uz);
        }

        // write end nullptr
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64_unchecked<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>(
            memory,
            environ_curr_size,
            ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t{});

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

