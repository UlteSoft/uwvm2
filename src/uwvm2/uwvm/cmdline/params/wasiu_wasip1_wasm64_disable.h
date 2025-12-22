/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-10-01
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
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
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
        inline constexpr ::uwvm2::utils::container::u8string_view wasiu_wasip1_wasm64_disable_alias{u8"-Iui1w64disable"};
        extern "C++" ::uwvm2::utils::cmdline::parameter_return_type
            wasiu_wasip1_wasm64_disable_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                 ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                 ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
    }  // namespace details

#  if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wbraced-scalar-init"
#  endif
    inline constexpr ::uwvm2::utils::cmdline::parameter wasiu_wasip1_wasm64_disable{
        .name{u8"--wasiu-wasip1-wasm64-disable"},
        .describe{u8"Disable preloading of the local WASIU-WASI-Preview1-WASM64 module (if loaded, it shares the environment with wasip1)."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::wasiu_wasip1_wasm64_disable_alias), 1uz}},
        .handle{::std::addressof(details::wasiu_wasip1_wasm64_disable_callback)},
        .cate{::uwvm2::utils::cmdline::categorization::wasi}};
#  if defined(__clang__)
#   pragma clang diagnostic pop
#  endif

# endif
#endif
}

#ifndef UWVM_MODULE
// macro
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
