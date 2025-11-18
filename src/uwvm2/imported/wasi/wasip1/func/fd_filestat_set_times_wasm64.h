

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
# if (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && !defined(__MSDOS__) && __has_include(<dirent.h>) && !defined(_PICOLIBC__)
#  include <unistd.h>
#  include <errno.h>
#  include <sys/stat.h>
#  include <sys/time.h>
# endif
# if defined(__MSDOS__) || defined(__DJGPP__)
#  include <utime.h>
# endif
// import
# include <fast_io.h>
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
# include "fd_filestat_set_times.h"
#endif

#ifndef UWVM_CPP_EXCEPTIONS
# warning "Without enabling C++ exceptions, using this WASI function may cause termination."
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{
    /// @brief     WasiPreview1.fd_filestat_set_times_wasm64
    /// @details   __wasi_errno_t fd_filestat_set_times_wasm64(__wasi_fd_t fd, __wasi_timestamp_t atim, __wasi_timestamp_t mtim, __wasi_fstflags_t fstflags);
    inline ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t fd_filestat_set_times_wasm64(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t atim,
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t mtim,
        ::uwvm2::imported::wasi::wasip1::abi::fstflags_wasm64_t fstflags) noexcept
    {
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
                                u8"fd_filestat_set_times_wasm64",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                fd,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(fstflags)>>>(fstflags),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
#else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: fd_filestat_set_times_wasm64(",
                                fd,
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim),
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim),
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(fstflags)>>>(fstflags),
                                u8") (wasi-trace)\n");
#endif
        }

        return ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_times_base(env, fd, atim, mtim, fstflags);
    }
}  // namespace uwvm2::imported::wasi::wasip1::func

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

