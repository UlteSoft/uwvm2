/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-27
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
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
    namespace details
    {
        inline constexpr ::uwvm2::utils::container::u8string_view wasi_disable_utf8_check_alias{u8"-Iu8relax"};
        inline constexpr ::uwvm2::utils::cmdline::parameter_return_type
            wasi_disable_utf8_check_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                             ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                             ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
    }  // namespace details

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wbraced-scalar-init"
#endif
    inline constexpr ::uwvm2::utils::cmdline::parameter wasi_disable_utf8_check{
        .name{u8"--wasi-disable-utf8-check"},
        .describe{u8"Disable the UTF-8 check for WASI (All versions)."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::wasi_disable_utf8_check_alias), 1uz}},
        .handle{::std::addressof(details::wasi_disable_utf8_check_callback)},
        .is_exist{::std::addressof(::uwvm2::uwvm::wasm::storage::wasi_disable_utf8_check)},
        .cate{::uwvm2::utils::cmdline::categorization::wasi}};
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
