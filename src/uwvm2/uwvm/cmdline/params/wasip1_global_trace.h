/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-04-26
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
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)
    namespace details
    {
        inline bool wasip1_global_trace_is_exist{};  // [global]
        inline constexpr ::uwvm2::utils::container::array<::uwvm2::utils::container::u8string_view, 2uz> wasip1_global_trace_alias{u8"--wasip1-trace", u8"-I1trace"};
#  if defined(UWVM_MODULE)
        extern "C++"
#  else
        inline constexpr
#  endif
            ::uwvm2::utils::cmdline::parameter_return_type wasip1_global_trace_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                 ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                 ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
    }  // namespace details

#  if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wbraced-scalar-init"
#  endif
    inline constexpr ::uwvm2::utils::cmdline::parameter wasip1_global_trace{
        .name{u8"--wasip1-global-trace"},
        .describe{u8"Route global-default WASI Preview 1 trace output to stdout, stderr, or a dedicated file."},
        .usage{
#  if !defined(__AVR__) && !((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) && !(defined(__MSDOS__) || defined(__DJGPP__)) &&             \
      !(defined(__NEWLIB__) && !defined(__CYGWIN__)) && !defined(_PICOLIBC__) && !defined(__wasm__)
            u8"[none|out|err|file <file:path>]"
#  else
            u8"[none|out|err]"
#  endif
        },
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{details::wasip1_global_trace_alias.data(), details::wasip1_global_trace_alias.size()}},
        .handle{::std::addressof(details::wasip1_global_trace_callback)},
        .is_exist{::std::addressof(details::wasip1_global_trace_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasi}};
#  if defined(__clang__)
#   pragma clang diagnostic pop
#  endif

# endif
#endif
}  // namespace uwvm2::uwvm::cmdline::params

#ifndef UWVM_MODULE
// macro
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
